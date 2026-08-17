#include "flight_comm.h"

#include "car.h"
#include "main.h"
#include "rc_control.h"
#include "usart.h"

#define FLIGHT_FRAME_HEADER_0       0xAAU
#define FLIGHT_FRAME_HEADER_1       0x55U
#define FLIGHT_PROTOCOL_VERSION     0x01U
#define FLIGHT_FRAME_SIZE           11U
#define FLIGHT_CRC_DATA_SIZE        10U
#define FLIGHT_DMA_RX_BUFFER_SIZE   128U
#define FLIGHT_INPUT_MIN            (-1000)
#define FLIGHT_INPUT_MAX            1000
#define FLIGHT_LINK_TIMEOUT_MS      500U
#define FLIGHT_DIAGNOSTIC_TIMEOUT_MS 20U

static uint8_t flight_dma_rx_buffer[FLIGHT_DMA_RX_BUFFER_SIZE];
static uint16_t flight_dma_read_position = 0U;

static uint8_t flight_reply_ok[] = "OK\r\n";
static uint8_t flight_reply_crc_error[] = "CRC_ERR\r\n";

static uint8_t flight_parser_buffer[FLIGHT_FRAME_SIZE];
static uint8_t flight_parser_count = 0U;

static FlightCarCommand flight_command;
static uint32_t flight_last_valid_packet_ms = 0U;
static uint32_t flight_command_generation = 0U;
static uint32_t flight_backend_generation = 0U;
static uint8_t flight_has_valid_packet = 0U;

static uint8_t flight_crc8(const uint8_t *data, uint16_t length)
{
  uint8_t crc = 0U;
  uint16_t index;
  uint8_t bit;

  for (index = 0U; index < length; index++)
  {
    crc ^= data[index];
    for (bit = 0U; bit < 8U; bit++)
    {
      if ((crc & 0x80U) != 0U)
      {
        crc = (uint8_t)((uint8_t)(crc << 1U) ^ 0x07U);
      }
      else
      {
        crc = (uint8_t)(crc << 1U);
      }
    }
  }

  return crc;
}

static int16_t flight_clamp_input(int16_t value)
{
  if (value > FLIGHT_INPUT_MAX)
  {
    value = FLIGHT_INPUT_MAX;
  }
  else if (value < FLIGHT_INPUT_MIN)
  {
    value = FLIGHT_INPUT_MIN;
  }

  return value;
}

static int16_t flight_decode_i16_be(uint8_t high_byte, uint8_t low_byte)
{
  uint16_t raw = ((uint16_t)high_byte << 8U) | (uint16_t)low_byte;
  return (int16_t)raw;
}

static void flight_send_diagnostic(uint8_t *message, uint16_t length)
{
  (void)HAL_UART_Transmit(&huart2,
                          message,
                          length,
                          FLIGHT_DIAGNOSTIC_TIMEOUT_MS);
}

static void flight_set_disconnected(void)
{
  uint8_t was_connected = flight_command.connected;

  flight_command.throttle = 0;
  flight_command.steering = 0;
  flight_command.mode = 0U;
  flight_command.enable = 0U;
  flight_command.connected = 0U;

  if (was_connected != 0U)
  {
    flight_command_generation++;
  }

  car_stop();
}

static uint8_t flight_validate_and_apply_frame(const uint8_t *frame)
{
  FlightCarCommand new_command;

  if ((frame[0] != FLIGHT_FRAME_HEADER_0) ||
      (frame[1] != FLIGHT_FRAME_HEADER_1))
  {
    return 0U;
  }

  if (flight_crc8(frame, FLIGHT_CRC_DATA_SIZE) != frame[10])
  {
    flight_send_diagnostic(flight_reply_crc_error,
                           (uint16_t)(sizeof(flight_reply_crc_error) - 1U));
    return 0U;
  }

  if ((frame[2] != FLIGHT_PROTOCOL_VERSION) ||
      (frame[3] > 1U) ||
      (frame[4] > 1U))
  {
    return 0U;
  }

  new_command.enable = frame[3];
  new_command.mode = frame[4];
  new_command.throttle = flight_clamp_input(
      flight_decode_i16_be(frame[5], frame[6]));
  new_command.steering = flight_clamp_input(
      flight_decode_i16_be(frame[7], frame[8]));
  new_command.connected = 1U;

  flight_command = new_command;
  flight_last_valid_packet_ms = HAL_GetTick();
  flight_has_valid_packet = 1U;
  flight_command_generation++;
  flight_send_diagnostic(flight_reply_ok,
                         (uint16_t)(sizeof(flight_reply_ok) - 1U));
  return 1U;
}

static void flight_parser_resync(void)
{
  uint8_t start;
  uint8_t index;
  uint8_t remaining;

  for (start = 1U; (uint8_t)(start + 1U) < flight_parser_count; start++)
  {
    if ((flight_parser_buffer[start] == FLIGHT_FRAME_HEADER_0) &&
        (flight_parser_buffer[start + 1U] == FLIGHT_FRAME_HEADER_1))
    {
      remaining = (uint8_t)(flight_parser_count - start);
      for (index = 0U; index < remaining; index++)
      {
        flight_parser_buffer[index] = flight_parser_buffer[start + index];
      }
      flight_parser_count = remaining;
      return;
    }
  }

  if (flight_parser_buffer[flight_parser_count - 1U] ==
      FLIGHT_FRAME_HEADER_0)
  {
    flight_parser_buffer[0] = FLIGHT_FRAME_HEADER_0;
    flight_parser_count = 1U;
  }
  else
  {
    flight_parser_count = 0U;
  }
}

static void flight_parser_push(uint8_t byte)
{
  if (flight_parser_count == 0U)
  {
    if (byte == FLIGHT_FRAME_HEADER_0)
    {
      flight_parser_buffer[0] = byte;
      flight_parser_count = 1U;
    }
    return;
  }

  if (flight_parser_count == 1U)
  {
    if (byte == FLIGHT_FRAME_HEADER_1)
    {
      flight_parser_buffer[1] = byte;
      flight_parser_count = 2U;
    }
    else if (byte != FLIGHT_FRAME_HEADER_0)
    {
      flight_parser_count = 0U;
    }
    return;
  }

  flight_parser_buffer[flight_parser_count] = byte;
  flight_parser_count++;

  if (flight_parser_count == FLIGHT_FRAME_SIZE)
  {
    if (flight_validate_and_apply_frame(flight_parser_buffer) != 0U)
    {
      flight_parser_count = 0U;
    }
    else
    {
      /* Keep any embedded AA 55 suffix so a dropped byte cannot permanently
         misalign every following frame. */
      flight_parser_resync();
    }
  }
}

static void flight_process_dma_bytes(void)
{
  uint16_t write_position;

  if ((huart2.hdmarx == 0) ||
      (huart2.hdmarx->Instance != DMA1_Channel6))
  {
    return;
  }

  write_position = (uint16_t)(FLIGHT_DMA_RX_BUFFER_SIZE -
      __HAL_DMA_GET_COUNTER(huart2.hdmarx));
  if (write_position >= FLIGHT_DMA_RX_BUFFER_SIZE)
  {
    write_position = 0U;
  }

  while (flight_dma_read_position != write_position)
  {
    flight_parser_push(flight_dma_rx_buffer[flight_dma_read_position]);
    flight_dma_read_position++;
    if (flight_dma_read_position >= FLIGHT_DMA_RX_BUFFER_SIZE)
    {
      flight_dma_read_position = 0U;
    }
  }
}

void flight_comm_init(void)
{
  uint16_t index;

  for (index = 0U; index < FLIGHT_DMA_RX_BUFFER_SIZE; index++)
  {
    flight_dma_rx_buffer[index] = 0U;
  }

  flight_dma_read_position = 0U;
  flight_parser_count = 0U;
  flight_command.throttle = 0;
  flight_command.steering = 0;
  flight_command.mode = 0U;
  flight_command.enable = 0U;
  flight_command.connected = 0U;
  flight_last_valid_packet_ms = HAL_GetTick();
  flight_command_generation = 0U;
  flight_backend_generation = 0U;
  flight_has_valid_packet = 0U;

  car_stop();

  if (HAL_UART_Receive_DMA(&huart2,
                           flight_dma_rx_buffer,
                           FLIGHT_DMA_RX_BUFFER_SIZE) != HAL_OK)
  {
    Error_Handler();
  }
}

void flight_comm_update(void)
{
  uint32_t now;

  flight_process_dma_bytes();
  now = HAL_GetTick();

  if ((flight_has_valid_packet == 0U) ||
      ((uint32_t)(now - flight_last_valid_packet_ms) >
       FLIGHT_LINK_TIMEOUT_MS))
  {
    flight_has_valid_packet = 0U;
    flight_set_disconnected();
  }
}

FlightCarCommand flight_comm_get_command(void)
{
  return flight_command;
}

uint8_t flight_comm_is_connected(void)
{
  return flight_command.connected;
}

#if FLIGHT_COMM_PROVIDE_RC_BACKEND
uint8_t rc_backend_read(RcCommand *command)
{
  FlightCarCommand source;

  if (command == 0)
  {
    return 0U;
  }

  flight_comm_update();

  if (flight_backend_generation == flight_command_generation)
  {
    return 0U;
  }

  flight_backend_generation = flight_command_generation;
  source = flight_comm_get_command();
#if FLIGHT_COMM_ENABLE_CAR_OUTPUT
  command->throttle = source.throttle;
  command->steering = source.steering;
  command->mode = source.mode;
  command->enable = source.enable;
#else
  command->throttle = 0;
  command->steering = 0;
  command->mode = 0U;
  command->enable = 0U;
#endif
  command->connected = source.connected;
  return 1U;
}
#endif
