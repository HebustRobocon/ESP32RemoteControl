file(GLOB PAGE_SRCS "lwpage.c" "mainpage.c" "page_manager.c" "*.c")
idf_component_register(SRCS "${PAGE_SRCS}"
    INCLUDE_DIRS "."
    REQUIRES driver lvgl freertos fatfs core screen
    LDFRAGMENTS "ui_page.lf"
)


foreach(src ${PAGE_SRCS})
    # 获取文件名（不带后缀），如 page_wifi
    get_filename_component(file_id ${src} NAME_WE)
    
    # --- 核心改进：预扫描文件内容 ---
    # 查找文件中是否包含 "UI_PAGE_REGISTER" 字符串
    file(STRINGS "${src}" has_reg REGEX "UI_PAGE_REGISTER")

    if(has_reg)
        # 只有包含宏的文件才进行以下操作
        set(target_symbol "${file_id}_page_port")

        # 注入编译宏 THIS_PAGE_ID
        set_source_files_properties(${src} PROPERTIES 
            COMPILE_DEFINITIONS "THIS_PAGE_ID=${file_id}")

        # 注入链接器强制拉取参数
        target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,-u,${target_symbol}")
        
        message(STATUS "UI Auto-Link [Found]: ${file_id} -> ${target_symbol}")
    else()
        # 不包含注册宏的文件正常编译，但不强制链接符号
        message(STATUS "UI Auto-Link [Skip ]: ${file_id} (No registration macro)")
    endif()
endforeach()
