# 计划

限定四个源码/测试文件。使用独立构建目录，专项验证worker_faults/jobs/host_crash_tree和正常Python业务，避免占用主线程构建产物。主线程其他失败不在本次修改范围，不替换latest包。
