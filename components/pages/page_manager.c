#include "page_manager.h"
#include "lvgl.h"

// 当前页面的画布容器
static lv_obj_t *current_page_canvas = NULL;

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
            printf("Found page: %s\r\n", next_page_name);
            page_create_info_stack[page_create_info_index].port_addr=this_page_port;
            page_create_info_stack[page_create_info_index].page_create_param=this_page_create_param;
            page_create_info_index++;

            this_page_port=page;
            printf("Creating new page canvas\r\n");
            // 创建新的画布容器，覆盖整个屏幕
            current_page_canvas = lv_obj_create(lv_screen_active());
            lv_obj_set_size(current_page_canvas, lv_pct(100), lv_pct(100));
            lv_obj_set_pos(current_page_canvas, 0, 0);
            // 设置画布为当前活动对象，新创建的UI元素会自动添加到这个画布中
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
    this_page_port=page_create_info_stack[page_create_info_index].port_addr;
    printf("Removing current page canvas\r\n");
    // 删除当前画布，露出下面的页面
    if(current_page_canvas != NULL)
    {
        lv_obj_del(current_page_canvas);
        current_page_canvas = NULL;
    }
    // 重新加载主屏幕，确保下面的页面可见
    lv_scr_load(lv_screen_active());
    this_page_port->create(page_create_info_stack[page_create_info_index].page_create_param);   //执行创建上一个页面
    page_create_info_index--;
    return 1;
}
