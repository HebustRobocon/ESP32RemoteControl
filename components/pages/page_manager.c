#include "page_manager.h"
#include "lvgl.h"

// 当前页面的画布容器
static lv_obj_t *current_page_canvas = NULL;
static lv_obj_t *page_canvas_stack[16] = {0};

//最多支持16个页面递归
typedef struct{
    const PagePort_t *port_addr;
    void* page_create_param;
}PageCreateInfo_t;

static PageCreateInfo_t page_create_info_stack[16];
static uint16_t page_create_info_index=0;
static const PagePort_t *this_page_port;

static QueueHandle_t kScreen_mutex;

//加载主页面
uint32_t page_manager_init(const char *first_page_name,QueueHandle_t screen_mutex)
{
    kScreen_mutex=screen_mutex;
    int page_num=get_pages_num();
    printf("page_num=%d\r\n",page_num);
    for(int i=0;i<page_num;i++)
    {
        if(strcmp(first_page_name,get_pages_from_index(i)->page_name)==0) //匹配到目标页面
        {
            this_page_port=get_pages_from_index(i);
            current_page_canvas = lv_scr_act();
            if(current_page_canvas == NULL)
            {
                current_page_canvas = lv_obj_create(NULL);
                lv_scr_load(current_page_canvas);
                printf("page_manager_init: created fallback initial screen %p\r\n", (void*)current_page_canvas);
            }
            xSemaphoreTake(get_screen_mutex(),portMAX_DELAY);
            this_page_port->create(NULL);   //该任务没有运行在lv线程中，而是在另一个线程执行，所以用互斥锁保护
            xSemaphoreGive(get_screen_mutex());
            return 1;
        }
    }
    return 0;
}

QueueHandle_t get_screen_mutex()
{
    return kScreen_mutex;
}

//切换到目标页面，同时将本页面重新创建信息入栈
uint32_t page_switch(const char* next_page_name,void *next_page_create_param,void* this_page_create_param)
{
    printf("page_switch called with page name: %s\r\n", next_page_name);
    uint32_t page_num=get_pages_num();
    printf("Total pages: %lu\r\n", page_num);
    for(int i=0;i<page_num;i++)
    {
        PagePort_t *page = get_pages_from_index(i);
        printf("Checking page %d: %s\r\n", i, page->page_name);
        if(strcmp(next_page_name, page->page_name)==0) //匹配到目标页面
        {
            if(page_create_info_index >= sizeof(page_create_info_stack) / sizeof(page_create_info_stack[0]))
            {
                printf("page_switch failed: page stack overflow\r\n");
                return 0;
            }

            printf("Found page: %s\r\n", next_page_name);
            // 保存当前页面屏幕，后退时恢复
            page_canvas_stack[page_create_info_index] = lv_scr_act();
            page_create_info_stack[page_create_info_index].port_addr = this_page_port;
            page_create_info_stack[page_create_info_index].page_create_param = this_page_create_param;
            page_create_info_index++;

            this_page_port = page;
            printf("Creating new page canvas\r\n");
            // 创建新的独立屏幕，并加载它
            current_page_canvas = lv_obj_create(NULL);
            lv_scr_load(current_page_canvas);
            printf("Calling create function for page: %s\r\n", next_page_name);
            this_page_port->create(next_page_create_param);
            printf("Page created successfully\r\n");
            return 1;
        }
    }
    printf("Page not found: %s\r\n", next_page_name);
    return 0;
}

//跳转到上一个页面
uint32_t return_last_page()
{
    if(!page_create_info_index)
        return 0;
    // 先递减索引，再访问栈元素，避免越界
    page_create_info_index--;
    this_page_port = page_create_info_stack[page_create_info_index].port_addr;
    if (!this_page_port) {
        printf("return_last_page failed: saved page port is NULL\r\n");
        return 0;
    }
    printf("Removing current page canvas\r\n");
    if(current_page_canvas != NULL)
    {
        lv_obj_t *old_screen = current_page_canvas;
        current_page_canvas = NULL;

        // 恢复上一个屏幕
        lv_obj_t *prev_screen = page_canvas_stack[page_create_info_index];
        printf("return_last_page: prev_screen=%p old_screen=%p active_before=%p\r\n",
               (void*)prev_screen, (void*)old_screen, (void*)lv_scr_act());
        if(prev_screen != NULL)
        {
            lv_scr_load(prev_screen);
            page_canvas_stack[page_create_info_index] = NULL;
        }
        else
        {
            printf("return_last_page: prev_screen is NULL, creating fallback screen\r\n");
            prev_screen = lv_obj_create(NULL);
            lv_scr_load(prev_screen);
        }
        printf("return_last_page: active_after_load=%p\r\n", (void*)lv_scr_act());

        if(old_screen != prev_screen)
        {
            // 延迟删除旧屏幕，避免在当前事件回调或渲染过程中删除活动屏幕
            lv_obj_del_async(old_screen);
        }
    }
    return 1;
}
