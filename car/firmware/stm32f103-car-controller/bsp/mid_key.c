#include "mid_key.h"
#include "string.h"

#include "bsp_key.h"
#include "app_key.h"
//定义用户按键
typedef enum
{
    BUTTON_UP = 0,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_DOWN,
    BUTTON_MID,
    USER_BUTTON_MAX
} user_button_t;

static flex_button_t user_button[USER_BUTTON_MAX];

static uint8_t common_btn_read(void *arg)
{
    uint8_t value = 0;

    flex_button_t *btn = (flex_button_t *)arg;

    switch (btn->id)
    {
      case BUTTON_UP:
        value = Key_Scan().key_up;
        break;
      case BUTTON_LEFT:
        value = Key_Scan().key_left;
        break;
      case BUTTON_RIGHT:
        value = Key_Scan().key_right;
        break;
      case BUTTON_DOWN:
        value = Key_Scan().key_down;
        break;
      case BUTTON_MID:
        value = Key_Scan().key_mid;
        break;
    }

    return value;
}



void button_scan(void *arg)
{
    while(1)
    {
        flex_button_scan();
    }
}
void user_button_init(void)
{
    int i;
    
    memset(&user_button[0], 0x0, sizeof(user_button));
    user_button[BUTTON_UP].cb    = up_btn_evt_cb;
    user_button[BUTTON_DOWN].cb  = down_btn_evt_cb;
    user_button[BUTTON_MID].cb   = mid_btn_evt_cb;
    user_button[BUTTON_RIGHT].cb = right_btn_evt_cb;
    user_button[BUTTON_LEFT].cb  = left_btn_evt_cb;
    for (i = 0; i < USER_BUTTON_MAX; i ++)
    {
        user_button[i].id = i;
        user_button[i].usr_button_read = common_btn_read;
        user_button[i].pressed_logic_level = 0;
        user_button[i].short_press_start_tick = FLEX_MS_TO_SCAN_CNT(1000);
        user_button[i].long_press_start_tick = FLEX_MS_TO_SCAN_CNT(1500);
        user_button[i].long_hold_start_tick = FLEX_MS_TO_SCAN_CNT(2000);

     
        flex_button_register(&user_button[i]);
    }
}

#ifndef NULL
#define NULL 0
#endif

#define EVENT_SET_AND_EXEC_CB(btn, evt)                                        \
    do                                                                         \
    {                                                                          \
        btn->event = evt;                                                      \
        if(btn->cb)                                                            \
            btn->cb((flex_button_t*)btn);                                      \
    } while(0)

/**
 * BTN_IS_PRESSED
 * 
 * 1: is pressed
 * 0: is not pressed
*/
#define BTN_IS_PRESSED(i) (g_btn_status_reg & (1 << i))

enum FLEX_BTN_STAGE
{
    FLEX_BTN_STAGE_DEFAULT = 0,
    FLEX_BTN_STAGE_DOWN    = 1,
    FLEX_BTN_STAGE_MULTIPLE_CLICK = 2
};

typedef uint32_t btn_type_t;

static flex_button_t *btn_head = NULL;

/**
 * g_logic_level
 * 
 * The logic level of the button pressed, 
 * Each bit represents a button.
 * 
 * First registered button, the logic level of the button pressed is 
 * at the low bit of g_logic_level.
*/
btn_type_t g_logic_level = (btn_type_t)0;

/**
 * g_btn_status_reg
 * 
 * The status register of all button, each bit records the pressing state of a button.
 * 
 * First registered button, the pressing state of the button is 
 * at the low bit of g_btn_status_reg.
*/
btn_type_t g_btn_status_reg = (btn_type_t)0;

static uint8_t button_cnt = 0;

/**
 * @brief Register a user button
 * 
 * @param button: button structure instance
 * @return Number of keys that have been registered, or -1 when error
*/
int32_t flex_button_register(flex_button_t *button)
{
    flex_button_t *curr = btn_head;
    
    if (!button || (button_cnt > sizeof(btn_type_t) * 8))
    {
        return -1;
    }

    while (curr)
    {
        if(curr == button)
        {
            return -1;  /* already exist. */
        }
        curr = curr->next;
    }

    /**
     * First registered button is at the end of the 'linked list'.
     * btn_head points to the head of the 'linked list'.
    */
    button->next = btn_head;
    button->status = FLEX_BTN_STAGE_DEFAULT;
    button->event = FLEX_BTN_PRESS_NONE;
    button->scan_cnt = 0;
    button->click_cnt = 0;
    button->max_multiple_clicks_interval = MAX_MULTIPLE_CLICKS_INTERVAL;
    btn_head = button;

    /**
     * First registered button, the logic level of the button pressed is 
     * at the low bit of g_logic_level.
    */
    g_logic_level |= (button->pressed_logic_level << button_cnt);
    button_cnt ++;

    return button_cnt;
}

/**
 * @brief Read all key values in one scan cycle
 * 
 * @param void
 * @return none
*/
static void flex_button_read(void)
{
    uint8_t i;
    flex_button_t* target;

    /* The button that was registered first, the button value is in the low position of raw_data */
    btn_type_t raw_data = 0;

    for(target = btn_head, i = button_cnt - 1;
        (target != NULL) && (target->usr_button_read != NULL);
        target = target->next, i--)
    {
        raw_data = raw_data | ((target->usr_button_read)(target) << i);
    }

    g_btn_status_reg = (~raw_data) ^ g_logic_level;
}

/**
 * @brief Handle all key events in one scan cycle.
 *        Must be used after 'flex_button_read' API
 * 
 * @param void
 * @return Activated button count
*/
static uint8_t flex_button_process(void)
{
    uint8_t i;
    uint8_t active_btn_cnt = 0;
    flex_button_t* target;
    
    for (target = btn_head, i = button_cnt - 1; target != NULL; target = target->next, i--)
    {
        if (target->status > FLEX_BTN_STAGE_DEFAULT)
        {
            target->scan_cnt ++;
            if (target->scan_cnt >= ((1 << (sizeof(target->scan_cnt) * 8)) - 1))
            {
                target->scan_cnt = target->long_hold_start_tick;
            }
        }

        switch (target->status)
        {
        case FLEX_BTN_STAGE_DEFAULT: /* stage: default(button up) */
            if (BTN_IS_PRESSED(i)) /* is pressed */
            {
                target->scan_cnt = 0;
                target->click_cnt = 0;

                EVENT_SET_AND_EXEC_CB(target, FLEX_BTN_PRESS_DOWN);

                /* swtich to button down stage */
                target->status = FLEX_BTN_STAGE_DOWN;
            }
            else
            {
                target->event = FLEX_BTN_PRESS_NONE;
            }
            break;

        case FLEX_BTN_STAGE_DOWN: /* stage: button down */
            if (BTN_IS_PRESSED(i)) /* is pressed */
            {
                if (target->click_cnt > 0) /* multiple click */
                {
                    if (target->scan_cnt > target->max_multiple_clicks_interval)
                    {
                        EVENT_SET_AND_EXEC_CB(target, 
                            target->click_cnt < FLEX_BTN_PRESS_REPEAT_CLICK ? 
                                target->click_cnt :
                                FLEX_BTN_PRESS_REPEAT_CLICK);

                        /* swtich to button down stage */
                        target->status = FLEX_BTN_STAGE_DOWN;
                        target->scan_cnt = 0;
                        target->click_cnt = 0;
                    }
                }
                else if (target->scan_cnt >= target->long_hold_start_tick)
                {
                    if (target->event != FLEX_BTN_PRESS_LONG_HOLD)
                    {
                        EVENT_SET_AND_EXEC_CB(target, FLEX_BTN_PRESS_LONG_HOLD);
                    }
                }
                else if (target->scan_cnt >= target->long_press_start_tick)
                {
                    if (target->event != FLEX_BTN_PRESS_LONG_START)
                    {
                        EVENT_SET_AND_EXEC_CB(target, FLEX_BTN_PRESS_LONG_START);
                    }
                }
                else if (target->scan_cnt >= target->short_press_start_tick)
                {
                    if (target->event != FLEX_BTN_PRESS_SHORT_START)
                    {
                        EVENT_SET_AND_EXEC_CB(target, FLEX_BTN_PRESS_SHORT_START);
                    }
                }
            }
            else /* button up */
            {
                if (target->scan_cnt >= target->long_hold_start_tick)
                {
                    EVENT_SET_AND_EXEC_CB(target, FLEX_BTN_PRESS_LONG_HOLD_UP);
                    target->status = FLEX_BTN_STAGE_DEFAULT;
                }
                else if (target->scan_cnt >= target->long_press_start_tick)
                {
                    EVENT_SET_AND_EXEC_CB(target, FLEX_BTN_PRESS_LONG_UP);
                    target->status = FLEX_BTN_STAGE_DEFAULT;
                }
                else if (target->scan_cnt >= target->short_press_start_tick)
                {
                    EVENT_SET_AND_EXEC_CB(target, FLEX_BTN_PRESS_SHORT_UP);
                    target->status = FLEX_BTN_STAGE_DEFAULT;
                }
                else
                {
                    /* swtich to multiple click stage */
                    target->status = FLEX_BTN_STAGE_MULTIPLE_CLICK;
                    target->click_cnt ++;
                }
            }
            break;

        case FLEX_BTN_STAGE_MULTIPLE_CLICK: /* stage: multiple click */
            if (BTN_IS_PRESSED(i)) /* is pressed */
            {
                /* swtich to button down stage */
                target->status = FLEX_BTN_STAGE_DOWN;
                target->scan_cnt = 0;
            }
            else
            {
                if (target->scan_cnt > target->max_multiple_clicks_interval)
                {
                    EVENT_SET_AND_EXEC_CB(target, 
                        target->click_cnt < FLEX_BTN_PRESS_REPEAT_CLICK ? 
                            target->click_cnt :
                            FLEX_BTN_PRESS_REPEAT_CLICK);

                    /* swtich to default stage */
                    target->status = FLEX_BTN_STAGE_DEFAULT;
                }
            }
            break;
        }
        
        if (target->status > FLEX_BTN_STAGE_DEFAULT)
        {
            active_btn_cnt ++;
        }
    }
    
    return active_btn_cnt;
}

/**
 * flex_button_event_read
 * 
 * @brief Get the button event of the specified button.
 * 
 * @param button: button structure instance
 * @return button event
*/
flex_button_event_t flex_button_event_read(flex_button_t* button)
{
    return (flex_button_event_t)(button->event);
}

/**
 * flex_button_scan
 * 
 * @brief Start key scan.
 *        Need to be called cyclically within the specified period.
 *        Sample cycle: 5 - 20ms
 * 
 * @param void
 * @return Activated button count
*/
uint8_t flex_button_scan(void)
{
    flex_button_read();
    return flex_button_process();
}
