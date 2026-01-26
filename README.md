# README

> 这个严格意义上是打别的比赛用的，但是工程本身是一个配置好的，触类旁通的，故整理发出

## 具体文件树

```txt
root              
│                 │.clang-format 代码格式化设置 可以按照自己风格改
│                 │keilkill.bat  删除中间文件
│                 │README.md     本文件
│                 │其余为生成的文件
│                 |---
├─.vscode         |里面`launch`要修改`.elf`（调试）
│                 |`settings`根据自己电脑修改工具链位置（clangd）
│                 |`tasks`是终端执行的任务 看自己改不改了
├─build           |---
│  └─Debug        |这里面有一个`compile_commands.json`（clangd）
├─cmake           |---
│  └─stm32cubemx  |没病不用动
├─Core            |---
│  ├─Inc          |略
│  └─Src          |略
├─Drivers         |---
├─Flash           |个人写的烧录相关 修改flash的.elf内容（openocd） 有条件可以加除了dap之外的烧录器
├─Middlewares     |官方生成库，DSP 和 FreeRTOS
└─User            |---
    ├─Alg         |算法
    ├─Bsp         |板载支持包
    ├─Drv         |驱动层 /设备层
    ├─Mid         |中间层
    └─App         |应用层 / 混编接口层

```
> 对这几个层的理解是 板载支持包完全是按照芯片引脚等等信息来写一层封装，在BSP的基础上，一点点的封装出对应的驱动/设备Drv，然后多个驱动/设备Drv连接起来，这就是中间层Mid。应用层就是把Mid和Drv的东西都拎出来，变成具体的任务。算法Alg就是穿插在这里面的。
>
> 也就是说，移植工程只需重写BSP的接口，就可以完整的用上之前的工程。那就得做到尽可能的解耦，以及树状结构，还有良好的封装。
>
> 会存在freertos的组成部分的，相信后人的智慧（App层用的多 可能其他的也要用）

## 使用说明

配置和编译应该是使用CMake官方的按钮，当然大部分时候是CMake会自动配置

配置和编译：CMake处有固定的命令：（自行添加，官方提供了）
- 使用cmake栏中的上半部分的内容也可以
- 配置
- 生成
- 清除重新生成
- 清楚缓存并重新配置 

烧录可以使用VSCode官方的Task，使用的openocd实现。烧录时只需要执行这个任务就可以

调试使用vscode的运行与调试栏

## 如何开发

1. 修改成适合自己电脑的一些配置文件
2. 严格按照四层结构去写，然后写完整的doxygen注释 （每个人的代码都有自己的小习惯 在每个文件开头注明author等等）
3. 编译烧录调试都使用vscode中的内容
4. 看明白这俩cmake（生成的），以及这些东西都是干啥的
5. 规范书写，多用git
6. 学会c/cpp混合编程，会写类，能面向对象编程

### 因为底层驱动涉及cpp的class等东西，有两种解决方案

1. 让main.c以cpp格式编译，所有问题不需要管，但是你只能在官方生成的代码里面的，main.cpp中进行操作，如果使用freertos同理
2. 严格在每一个.c .cpp中写接口转接文档，每个不能调用的东西都进行接口转接。

我这边选择的第二个

写接口转接文档，发现在嵌入式中，只需要给`main.c`进行转接，使用`api_main`来当作提取出来的main即可。这样写的中断回调 初始化 while循环都没啥问题

HAL库也早就写好了cpp调用c的`extern "C"`内容

作为c调用cpp的转接cpp文件的就写`api_xxxx.cpp`和`api_xxx.h` 作为区分 用**`h`**

正常cpp文件为：`xxx_yyyy.cpp` 和 `xxx_yyyy.hpp` 作为区分 用**`hpp`**

`xxx为 app mid bsp drv alg`

多去试试新的写法

**最终迭代成使用CMAKE，工程使用C++书写，开盒即用**

# NOTE

USART1 为经过了CH340的type-c接口 可以直连上位机

USART5 为接收机 只有RX端

两个继电器BREAKER控制XT30的座子是否导通 靠近电源的为1

三路FDCAN

四个舵机口

有SPI Flash

