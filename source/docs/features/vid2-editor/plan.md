# 实施设计
宿主1.6.0汇集未发布UIR11，视频工具1.4.1，既有桥1/state1；工具内部editorVersion=2识别旧单选区，未来未知状态只读不覆盖。所有视频业务在独立工具；后端扩展prepare/export及thumbnails/snapshot/batch-export/batch-status。批作业预校验全部并一次预约目标，逐文件沿用原临时记录/只读身份/原子不覆盖提交，提交后恢复可取消阶段，子结果写工具自有journal。禁止前端定时器拥有执行队列。GIF限制30秒/≤15fps/≤640边。文件名/格式/码率/时间强校验，CRF不虚报大小。缩略图16帧小JPEG/dataURL，上限缓存32份/64MiB，可清理非业务缓存。
