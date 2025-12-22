#include <iostream>
#include <spdlog/spdlog.h>
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
// TIP 要<b>Run</b>代码，请按 <shortcut actionId="Run"/> 或点击装订区域中的 <icon src="AllIcons.Actions.Execute"/> 图标。
int main() {
    // 初始化一个log对象
    auto logger = spdlog::basic_logger_mt("global_logger", "/Users/dxm/work/memory_pool_xs/log/memory_pool.log");

    // sink添加stdout
    logger->sinks().push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    // 定制日志格式
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%@] [%l] %v");
    
    // 测试输出日志
    logger->info("test");
    SPDLOG_LOGGER_INFO(logger, "test");
    SPDLOG_LOGGER_WARN(logger, "warning");

    // logger->flush();
    // spdlog::shutdown();
}