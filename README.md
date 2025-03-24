## 主线任务

### 对POSIX支持的远期目标
- [ ] 完善文件局部锁（需要进程的阻塞和事件机制支持）
- [ ] 在dentry.c中做完备的跨挂载点支持
- [ ] dentry的符号链接解析器需要重做，（需要完善的path作为支持）
- [ ] 实现通用的事件队列，并监听dentry的变化
- [ ] 在task_struct和scheduler中加入进程cpu亲和性
- [ ] 重写进程信号量
- [ ] 信号量的队列需要用list重写
- [ ] 实现io多路复用
- [ ] 在fs_struct中初始化mnt_ns


### 目前正在着手的任务
- [x] 在内核页表中分配一整段虚拟内存
- [x] 完善inode  
- [x] 重写inode hash
- [ ] 在路径遍历中处理挂载点
- [ ] 尚未实现vfs文件系统、设备、虚拟标准输入输出表述符、主要系统调用。
- [ ] 在file_open中做文件创建
- [ ] 在`__file_free`中处理f_private字段
- [x] 用嵌入式哈希表重写inode hash



### 可选任务
- [ ] 在vfs中实现读写锁
- [ ] 在I_FREEING时释放inode
- [ ] 实现懒挂载
- [ ] dentry的快照与缓存一致性
- [ ] 加入一个现代的debugger
- [ ] 在kmalloc中加入gfp_flags
- [ ] 重写上下文服务程序

## 小任务
- [ ] 把readp和writep分离出去
- [x] file_llseek中没有用到kiocb
- [x] 把命名标准写成文档
- [ ] 为每个cpu准备一个idle task
- [ ] 在user_stack创建和销毁的时候，需要对栈顶-16
- [x] 实现基数树
- [x] 在struct file中，只保留path字段
- [x] 重做使用链表处理碰撞的哈希表
- [x] 实现dentry lru机制
- [x] 设计一个dentry专用的哈希表
- [x] 在dentry中额外维护成员表
- [x] fdtable的size要能动态扩展
- [x] 在kstack启用和销毁的时候预先-16
- [x] 需要实现pid的分配
- [ ] 在inode_create中匹配成员字段
