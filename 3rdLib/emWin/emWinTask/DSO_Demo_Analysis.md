# emWin DSO 示例绘制分析

## 1. 说明

这份文档是针对当前工程里的 emWin 示波器示例做的源码级分析，目标是把下面几件事讲清楚：

1. 这个示波器界面到底是怎么画出来的。
2. 它的函数调用顺序是什么。
3. 哪些部分是静态框架，哪些部分是动态波形，哪些部分是交互层。
4. 它各个功能模块分别负责什么。
5. 后续如果要搬到 AppWizard 的 `slot` 文件里，应该保留什么结构，按什么风格接入。

先说一个结论：

- 你提到的 `<virtual_root>/emwin/DSO_Demo` 在当前工程里不是一个真实目录。
- 实际对应的源码位于 `3rdLib/emWin/emWinTask/` 下的一组 `DSO_*.c` 和 `MainTask.c`。
- 这个示波器**主波形区不是用 `GRAPH` 控件画的**，而是自己手动画背景、手动画波形、手动画游标。
- `GRAPH` 只在 ADC / DAC 这类辅助界面里用到了，不是主示波器绘制方式。

这点对后面迁移到 AppWizard 很关键，因为后续不能简单理解成“放一个 `GRAPH` 控件就等于示波器”，它的核心其实是“自绘波形层”。

## 2. 涉及文件与职责划分

### 2.1 主入口与全局结构

- `3rdLib/emWin/emWinTask/MainTask.c`
  这是整个 DSO 示例的主调度文件。负责：
  - 初始化内存和全局状态
  - 初始化 emWin
  - 创建内存设备 `GUI_MEMDEV`
  - 预绘制背景
  - 创建主界面和透明触摸滑动层
  - 进入主循环
  - 触发 `_DrawWave()` 刷新波形

- `3rdLib/emWin/emWinTask/MainTask.h`
  这里定义了示波器的大部分公共数据结构、坐标宏、通道颜色、测量表、时基表、幅值表。

### 2.2 波形主绘制相关

- `3rdLib/emWin/emWinTask/DSO_DrawBakFrame.c`
  负责绘制示波器网格背景。

- `3rdLib/emWin/emWinTask/DSO_Init.c`
  负责绘制主框架静态内容，包括：
  - 顶部状态栏
  - 触发浏览条
  - 波形区边框
  - 左侧参考位置标记
  - 各类按钮、勾选框、单选框
  - 创建状态对话框与幅值对话框

- `3rdLib/emWin/emWinTask/DSO_DrawCursorH.c`
  绘制水平游标和游标测量信息框。

- `3rdLib/emWin/emWinTask/DSO_DrawCursorV.c`
  绘制垂直游标和游标测量信息框。

### 2.3 波形数据处理相关

- `3rdLib/emWin/emWinTask/DSO_WaveProcess.c`
  负责：
  - 从 ADC DMA 环形缓冲中截取数据
  - 做软件触发定位
  - 把要显示的数据装入 `usWaveBuf`
  - 计算最大值、最小值、平均值、峰峰值、RMS、频率
  - 可选 FIR 滤波

### 2.4 辅助窗口与功能页

- `DSO_AmplitudeDlg.c`
  幅值窗口

- `DSO_Status1Dlg.c`
  状态窗口 1

- `DSO_Status2Dlg.c`
  状态窗口 2

- `DSO_ScaleDlg.c`
  时基和比例窗口

- `DSO_SysInfoDlg.c`
  系统信息窗口

- `DSO_MeasureDlg.c`
  测量选项窗口

- `DSO_MathDlg.c`
  滤波/数学功能窗口

- `DSO_SettingDlg.c`
  设置窗口

- `DSO_AdcDlg.c`
  ADC 辅助页。这里用了 `GRAPH` 控件。

- `DSO_DacDlg.c`
  DAC 辅助页。这里也用了 `GRAPH` 控件。

## 3. 主界面的坐标与区域概念

在 `MainTask.h` 中，示波器波形区域定义如下：

- `DSOSCREEN_STARTX = 40`
- `DSOSCREEN_STARTY = 40`
- `DSOSCREEN_ENDX = 1039`
- `DSOSCREEN_ENDY = 639`
- `DSOSCREEN_LENGTH = 1000`

这意味着主波形区是一个从 `(40, 40)` 到 `(1039, 639)` 的矩形。

可以把整个示波器界面理解成下面四层：

1. 桌面主框架层
   负责整体背景、标题、按钮、状态文字、波形区外框。

2. 波形背景缓存层
   负责示波器网格，提前画到 `GUI_MEMDEV` 里，避免每帧重画静态网格。

3. 动态波形层
   负责每次刷新时重画波形、游标、触发箭头。

4. 透明交互层
   负责拖动操作，不直接显示内容，但接收触摸/滑动并修改参考位置或触发浏览位置。

这个分层思想，就是后续迁移到 AppWizard 时最值得保留的部分。

## 4. 总体函数调用顺序

下面按“程序启动后实际发生的顺序”来讲。

### 4.1 顶层顺序

主入口在 `MainTask.c` 的 `MainTask()`：

1. `osRtxMemoryInit(AppMallocCCM, 1024 * 80)`
   初始化一块 RTX 风格的动态内存池。

2. 依次申请多个核心对象
   - `g_DSO1`
   - `g_DSO2`
   - `g_Cursors`
   - `g_Flag`
   - `g_TrigVol`
   - FFT 缓冲
   - RMS 缓冲
   - FIR 缓冲

3. 初始化默认状态
   - 通道参考位置
   - 触发电平位置
   - 时基和幅值默认项
   - 游标默认位置
   - 默认开启哪些测量项
   - 运行/暂停标志
   - 触发模式标志
   - 波形刷新标志

4. `WM_SetCreateFlags(WM_CF_MEMDEV)`
   开启窗口内存设备机制。

5. `GUI_Init()`
   初始化 emWin。

6. `WM_SetCallback(WM_HBKWIN, _cbBkWin)`
   把桌面窗口回调设置为 `_cbBkWin`。

7. `WM_MOTION_Enable(1)`
   开启滑动支持。

8. `WM_MOTION_SetDefaultPeriod(40)`
   设置滑动采样周期。

9. 创建两个核心内存设备
   - `hMemDSO`
   - `hMemDSO1`

10. 预绘制静态背景到内存设备
    - `DSO_DrawBakFrame()`
    - `DrawWinSysBk()`
    - `DrawWinAmpBk()`
    - `DrawWinStatus1Bk()`
    - `DrawWinStatus2Bk()`
    - `DrawWinScaleBk()`

11. `WM_HideWin(WM_HBKWIN)`
    先隐藏背景窗口，避免界面分步出现时闪烁。

12. `DSO_Init(1)`
    进行完整主界面初始化，包括静态内容绘制和控件创建。

13. `WM_ShowWin(WM_HBKWIN)`
    显示背景窗口。

14. 创建透明运动窗口 `hMotion`
    这个窗口覆盖在波形区上方，专门处理滑动交互。

15. `GUI_Exec()`
    先执行一轮 GUI 消息。

16. `DSO_Graph()`
    进入示波器主循环。

### 4.2 主循环顺序

主循环在 `MainTask.c` 的 `DSO_Graph()`：

1. 创建桌面窗口定时器 `WM_CreateTimer(...)`
   代码注释写明它用于“触发电压参考线的短暂显示”。

2. 进入 `while(1)` 循环。

3. 判断是否需要重画波形
   条件是：
   - 运行态 `g_Flag->hWinRunStop == 0`
   - 或强制刷新 `g_Flag->ucWaveRefresh == 1`

4. 满足条件时调用 `_DrawWave()`
   这是最核心的动态绘制函数。

5. 刷新结束后把 `g_Flag->ucWaveRefresh = 0`

6. `GUI_Exec()`
   执行 GUI 消息调度。

7. `GUI_Delay(1)`
   做一个很短的延时。

8. 读取按键 `bsp_GetKey()`
   处理硬件按键逻辑。

也就是说，这个示波器不是“控件自动帮你刷波形”，而是它自己在主循环里周期性决定何时调用 `_DrawWave()`。

## 5. 主框架是怎么画出来的

主框架绘制主要发生在两处：

1. `DSO_Init(1)` 或 `DSO_Init(0)`
2. 桌面回调 `_cbBkWin()` 的 `WM_PAINT`

### 5.1 `_cbBkWin()` 的职责

桌面回调 `_cbBkWin()` 主要处理三类消息：

#### 1. `WM_PAINT`

收到桌面重绘消息时直接调用：

- `DSO_Init(0)`

这里的意思是：

- 只重新画静态框架
- 不重新创建控件

这是一个很常见的 emWin 设计方式：把“第一次完整初始化”和“后续重绘”共用同一个绘制函数，通过参数区分。

#### 2. `WM_TIMER`

收到定时器消息时：

- `g_Flag->ucWaveRefresh = 1`

作用是请求下一次波形刷新。

#### 3. `WM_NOTIFY_PARENT`

处理顶层按钮点击事件，比如：

- Measure
- ADC
- DAC
- Math
- Settings

点击后分别打开：

- `DSO_CreateMeasureDlg()`
- `DSO_CreateAdcDlg()`
- `DSO_CreateDacDlg()`
- `DSO_CreateMathDlg()`
- `DSO_CreateSettingsDlg()`

### 5.2 `DSO_Init()` 的详细绘制步骤

`DSO_Init(uint8_t ucCreateFlag)` 是整个示波器静态框架的核心。

它的绘制步骤可以按下面理解：

#### 第 1 步：清空主背景并贴出网格缓存

先清背景：

- `GUI_SetBkColor(BkColor)`
- `GUI_Clear()`

再把预先绘制好的网格背景贴到主屏幕：

- `GUI_MEMDEV_WriteAt(hMemDSO, 40, 40)`

这说明：

- 网格本身不是每次现画
- 而是提前画在 `hMemDSO` 这个内存设备里
- 重绘时直接贴图

这是示波器刷新流畅的关键步骤之一。

#### 第 2 步：绘制顶部固定信息

包括：

- 标题 `DSO3.0`
- Run / Stop
- Auto / Trig
- 触发电压文字
- 当前按键调节模式文字

涉及的绘制 API 主要有：

- `GUI_SetFont`
- `GUI_SetColor`
- `GUI_DispStringInRect`
- `GUI_DrawHLine`
- `GUI_DrawLine`

这里的顶部区域本质上是纯手工绘制文字和线段，不是一个独立 widget。

#### 第 3 步：绘制触发浏览条

它先清掉浏览条区域：

- `GUI_ClearRect(270, 6, 790, 33)`

再画一条黄色横线作为浏览带：

- `GUI_DrawHLine(20, 280, 280 + 499)`
- `GUI_DrawHLine(21, 280, 280 + 499)`

然后根据当前触发位置计算浏览箭头位置：

- 自动触发模式时，依据 `g_DSO1->sCurTriStep + g_DSO1->sCurTriPos`
- 普通触发模式时，依据 `724 + g_DSO1->sCurTriStep`

最后用：

- `GUI_FillPolygon(&aPointsTrigBrowser[0], ..., ulTrigPos + 280, 13)`

把小三角形箭头画出来。

这相当于一个“2K 原始数据窗口中的当前观察位置导航条”。

#### 第 4 步：绘制波形区边框

用两层矩形框给波形区包边：

- 一层浅灰 `0x888888`
- 一层深灰 `0x434343`

API 是：

- `GUI_DrawRect`

这一步把波形显示区域从周边控件区里“框”出来。

#### 第 5 步：首次初始化时创建各种控件和子窗口

当 `ucCreateFlag == 1` 时，会创建：

- 幅值窗口 `CreateAmplitudeDlg()`
- 状态窗口 1 `CreateStatus1Dlg()`
- 状态窗口 2 `CreateStatus2Dlg()`
- 一组按钮 `BUTTON_Create`
- 一组复选框 `CHECKBOX_CreateEx`
- 单选框 `RADIO_CreateEx`
- 触发按钮 `T`

也就是说：

- `DSO_Init(1)` = 画静态界面 + 创建子控件
- `DSO_Init(0)` = 只重画，不重新创建控件

这个拆法后面搬到 AppWizard 时要保留。

#### 第 6 步：绘制左侧通道参考位置箭头

左侧 1~8 通道的小箭头不是按钮，也不是图片资源，而是手工画的多边形：

- `GUI_FillPolygon(&aPoints[0], GUI_COUNTOF(aPoints), 5, y)`

然后再用：

- `GUI_DispCharAt('1'...'8', ...)`

写通道编号。

底部还有一个 `M` 标记，也是用多边形和字符手动画出来的。

这一部分说明作者更偏向“轻量自绘”，不是大量依赖资源图片。

## 6. 网格背景是怎么画出来的

网格由 `DSO_DrawBakFrame()` 绘制。

这个函数是理解示波器视觉效果的关键。

### 6.1 它不是画整条网格线，而是画点阵

函数先做：

- `GUI_SetBkColor(GUI_BLACK)`
- `GUI_ClearRect(x0, y0, x1, y1)`

然后用 `GUI_DrawPixel()` 一个点一个点地画刻度阵列。

### 6.2 水平方向的大刻度点

循环逻辑是：

- Y 方向：每隔 `50` 像素一行
- X 方向：每隔 `10` 像素一个点

也就是：

- 横向是密点
- 纵向是大格分段

### 6.3 竖直方向的大刻度点

循环逻辑是：

- X 方向：每隔 `50` 像素一列
- Y 方向：每隔 `10` 像素一个点

这样就形成了示波器典型的“虚点格栅”。

### 6.4 中心十字辅助线

函数还专门画了中间两条辅助中心线，但不是实线，而是“加粗点阵”：

- 垂直中心附近画在 `x0 + 499` 和 `x0 + 501`
- 水平中心附近画在 `y0 + 299` 和 `y0 + 301`

这样形成中轴参考线，视觉上比普通格点更显眼。

### 6.5 为什么它要提前画进 `GUI_MEMDEV`

因为网格是静态内容，如果每次刷新波形都重算一遍网格点，会浪费很多时间。

所以这里的设计是：

1. 启动时调用 `DSO_DrawBakFrame()`
2. 把结果画进 `hMemDSO`
3. 后续重绘时只做 `GUI_MEMDEV_WriteAt(hMemDSO, 40, 40)`

这就是“静态底图缓存”的思路。

## 7. 动态波形是怎么画出来的

动态波形全部集中在 `MainTask.c` 的 `_DrawWave()`。

这是整个示波器最核心的函数。

### 7.1 `_DrawWave()` 的函数顺序

按执行顺序，它大致是：

1. 设置剪裁区
   - `GUI_SetClipRect(&Rect)`

2. 选中动态波形内存设备
   - `GUI_MEMDEV_Select(hMemDSO1)`

3. 清空当前动态图层
   - `GUI_SetBkColor(GUI_BLACK)`
   - `GUI_Clear()`

4. 把波形数据转换成屏幕 Y 坐标

5. 逐通道调用 `GUI_DrawGraph()` 绘制波形

6. 如果开启游标，则调用：
   - `DSO_DrawCursorH()`
   - 或 `DSO_DrawCursorV()`

7. 绘制触发箭头与临时触发水平线

8. 取消内存设备选择
   - `GUI_MEMDEV_Select(0)`

9. 把动态图层贴回屏幕
   - `GUI_MEMDEV_WriteAt(hMemDSO1, 40, 40)`

10. 清除剪裁区
    - `GUI_SetClipRect(NULL)`

这就是完整的一次波形刷新流程。

### 7.2 这里用了双层缓存思想

虽然不是严格意义上的全双缓冲，但逻辑上它已经分成两套缓存：

- `hMemDSO`
  静态网格背景缓存

- `hMemDSO1`
  动态波形缓存

所以波形刷新时，作者不是在 LCD 上一笔一笔直接画，而是：

1. 先在内存设备里完成整帧动态内容
2. 再一次性贴到屏幕指定区域

这样做的好处是：

- 减少闪烁
- 降低撕裂感
- 绘制逻辑更集中

### 7.3 波形真正使用的绘制 API

主波形区最关键的 API 是：

- `GUI_DrawGraph((I16 *)buf, count, x0, y0)`

这说明作者已经把波形数据准备成了“按 X 递增的 Y 数组”。

也就是说：

- X 轴不需要每次手动传每个点
- 只要给出一段 Y 数组
- emWin 就会按连续样本把折线画出来

### 7.4 当前仓库里的一个重要现状

当前 `_DrawWave()` 里，真实的 ADC 缩放代码被注释掉了，实际是用 `rand()` 造了演示波形。

典型代码逻辑是：

- 原本想用 `g_DSO1->usWaveBuf[i] * 100 / g_AttTable[Ch1AmpId][0]`
- 当前改成了 `rand() % 10`

这意味着：

- 整个“绘制链路”是完整的
- 但当前仓库中的“波形数值来源”是占位演示，不是真实采样最终显示状态

这点非常重要。后续你移植示波器应用时：

- 背景怎么画，可以直接学
- 波形怎么刷，可以直接学
- 但数据换算和采样接入，不能照着当前 `_DrawWave()` 的 `rand()` 部分来用

## 8. 波形数据从哪里来

虽然当前 `_DrawWave()` 用了演示数据，但整个示波器的数据处理通道其实在 `DSO_WaveProcess.c` 里。

### 8.1 自动触发模式的数据流程

以 `DSO1_WaveTrig(uint16_t usCurPos)` 为例：

1. 根据 DMA 当前写指针 `usCurPos`
2. 从 10K 环形 ADC 缓冲里截出最近的 2K 数据
3. 放到 `g_DSO1->usWaveBufTemp`
4. 在前 `2048 - 600 = 1448` 个点里做触发点搜索
5. 找到合适触发点后，取出 600 点到 `g_DSO1->usWaveBuf`

也就是说：

- `usWaveBufTemp` 更像“中间采样窗口”
- `usWaveBuf` 才是“真正准备用来显示的一屏数据”

### 8.2 低速时基下的慢速刷新

当 `TimeBaseId >= 10` 时，代码进入慢速刷新模式：

- 利用 `ulSlowRefresh0`
- `ulSlowRefresh1`
- `ucSlowWaveFlag`

控制一个“从左到右推进，再转入整体刷新”的慢速显示过程。

这说明作者不是所有时基都用同一种刷新方式，而是针对低采样率专门做了滚动显示处理。

### 8.3 普通触发模式

普通触发模式下：

- 未触发时不持续刷新
- 触发后才允许一帧显示
- 然后再回到等待触发状态

这和自动触发模式的持续刷波形是不同的。

### 8.4 测量值与频率计算

`DSO1_WaveProcess()` / `DSO2_WaveProcess()` 里负责：

- 最大值 `WaveMax`
- 最小值 `WaveMin`
- 平均值 `WaveMean`
- 峰峰值 `WavePkPk`
- RMS `WaveRMS`
- 频率 `uiFreq`

频率的估算思路是：

1. 对 2048 点数据做 FFT
2. 找主峰位置
3. 再结合 `g_SampleFreqTable[TimeBaseId][0]` 换算出频率

不过当前 FFT 调用有一部分也是注释状态，说明这个工程里保留了结构，但不一定全部处于最终启用版本。

## 9. 游标和触发是怎么叠加上去的

### 9.1 触发箭头

触发箭头在 `_DrawWave()` 末尾绘制。

关键函数：

- `GUI_FillPolygon(&aPointsTrig[0], ..., 1027, g_TrigVol->usPos)`

这表示作者把触发箭头做成一个多边形模板，然后平移到波形区右边。

另外还有一条“临时触发电平线”：

- 当 `GUI_GetTime() - g_TrigVol->ulTrigTimeLine < 1000` 时
- 调用 `GUI_DrawHLine(g_TrigVol->usPos, 40, 1026)`

所以你可以把它理解成：

- 箭头是常驻视觉标记
- 水平线是调整触发电平后的短暂辅助线

### 9.2 水平游标 `DSO_DrawCursorH()`

这个函数实际画的是两条水平线，用于测量电压差。

它做了几件事：

1. 画两条水平线 A / B
2. 在线边画小圆角矩形标签
3. 标签里写 `a` 和 `b`
4. 右侧再画一个信息框
5. 显示 A、B 和差值
6. 同时把时间和电压两个维度都算出来显示

使用的典型 API 有：

- `GUI_DrawHLine`
- `GUI_FillRoundedRect`
- `GUI_AA_DrawRoundedRect`
- `GUI_DispStringAt`

### 9.3 垂直游标 `DSO_DrawCursorV()`

这个函数画两条垂直线，用于测量时间差。

它和水平游标的结构是一样的，只是：

- 线改成竖线
- 计算重点变成时间差

使用的典型 API 有：

- `GUI_DrawVLine`
- `GUI_FillRoundedRect`
- `GUI_DispStringAt`

### 9.4 一个值得注意的设计点

游标没有被拆成独立 widget，而是作为“动态波形层的一部分”在 `_DrawWave()` 里叠加。

这说明作者把“波形 + 游标 + 触发”视作一个完整的自绘画面，而不是让每个元素都变成单独控件。

这对 AppWizard 迁移也很重要：

- 后面我们最好把示波器波形区当作一个统一自绘区
- 不建议把游标再拆成很多零散 widget

## 10. 透明交互层是怎么工作的

这部分在 `MainTask.c` 的 `_cbMotion()`。

### 10.1 交互窗口的创建方式

主界面创建了一个透明子窗口：

- 位置 `(40, 40)`
- 大小 `1000 x 600`
- 覆盖整个波形区

创建函数：

- `WM_CreateWindowAsChild(...)`

标志位中包含：

- `WM_CF_MOTION_Y`
- `WM_CF_SHOW`
- `WM_CF_HASTRANS`

其中 `WM_CF_HASTRANS` 表示这个窗口自身透明，不影响底下波形显示。

### 10.2 `_cbMotion()` 的处理逻辑

在 `WM_MOTION` 里：

#### 1. `WM_MOTION_INIT`

设置支持：

- X 方向滑动
- Y 方向滑动

#### 2. `WM_MOTION_MOVE`

根据 `g_Flag->ucMotionXY` 决定当前滑动模式。

##### 模式 A：上下拖动参考位置

当 `g_Flag->ucMotionXY == 0` 时：

- 左半边拖动修改 CH1 参考位置 `g_DSO1->usRefPos`
- 右半边拖动修改 CH2 参考位置 `g_DSO2->usRefPos`
- 然后 `WM_InvalidateArea(&rRefPos)`

##### 模式 B：左右拖动浏览触发位置

当 `g_Flag->ucMotionXY == 1` 时：

- 修改 `g_DSO1->sCurTriStep`
- 普通触发模式下限制范围在 `[-724, 724]`
- 然后 `WM_InvalidateArea(&rTrigPos)`

最后无论哪种模式都会：

- `g_Flag->ucWaveRefresh = 1`

也就是说，透明窗口本身不画内容，它只负责把手势变成参数变化，然后请求主波形刷新。

## 11. 辅助对话框的绘制方式

这些对话框虽然不是主波形区，但很值得学，因为它们体现了作者的窗口组织习惯。

### 11.1 公共模式

以 `DSO_AmplitudeDlg.c`、`DSO_Status1Dlg.c`、`DSO_Status2Dlg.c` 等文件为例，普遍采用下面这个模式：

1. 定义 `GUI_WIDGET_CREATE_INFO` 数组
2. 通过 `*_CreateIndirect` 创建对话框
3. 在 `WM_INIT_DIALOG` 中初始化文字、颜色、字体
4. 在 `WM_PAINT` 中自绘背景
5. 有些窗口还用 `WM_TIMER` 或自定义 `WM_TextUpDate` 更新内容

### 11.2 背景预绘制

这些小窗口很多也用了 `GUI_MEMDEV_CreateFixed()`：

- `DrawWinAmpBk()`
- `DrawWinStatus1Bk()`
- `DrawWinStatus2Bk()`
- `DrawWinScaleBk()`
- `DrawWinSysBk()`

思路与主网格背景一致：

1. 先在内存设备里画好背景
2. 正式窗口需要重绘时再贴出来或直接调用同一套绘制函数

这说明作者在整个 UI 里都在贯彻一个思路：

- 静态背景缓存
- 动态文字/数据单独更新

## 12. `GRAPH` 控件到底用在什么地方

这是一个很容易误判的点，所以单独拿出来说明。

### 12.1 主示波器波形区没有用 `GRAPH`

主波形区核心函数是 `_DrawWave()`，核心 API 是：

- `GUI_DrawGraph`
- `GUI_DrawPixel`
- `GUI_DrawRect`
- `GUI_FillPolygon`

这里的 `GUI_DrawGraph` 不是 `GRAPH` 控件，它只是一个绘图 API。

### 12.2 `GRAPH` 只在辅助页里出现

在 `DSO_AdcDlg.c` 中：

- `GRAPH_DATA_YT_Create`
- `GRAPH_AttachData`
- `GRAPH_SCALE_Create`
- `GRAPH_AttachScale`

在 `DSO_DacDlg.c` 中：

- `GRAPH_DATA_YT_Create`
- `GRAPH_AttachData`

所以可以明确下结论：

- 主示波器应用的“架构参考”，要看 `MainTask.c + DSO_Init.c + DSO_DrawBakFrame.c`
- 辅助曲线窗口如果想快速做，可以参考 `DSO_AdcDlg.c / DSO_DacDlg.c` 的 `GRAPH` 写法

## 13. 这个示波器例程提供了哪些功能

从结构上看，它已经具备一个比较完整的数字示波器 UI 壳子。

### 13.1 主波形显示

- 最多显示多通道彩色波形
- 支持通道参考位置显示
- 支持自动触发和普通触发两种模式

### 13.2 触发浏览

- 顶部有触发浏览条
- 可以左右浏览 2K 原始采样窗口中的观察片段

### 13.3 游标测量

- 水平游标测电压差
- 垂直游标测时间差
- 显示 A、B、差值

### 13.4 数据测量

- 频率
- 峰峰值
- 最大值
- 最小值
- 平均值
- RMS
- 以及预留的更多测量项

### 13.5 数学处理

- FFT 结构预留
- FIR 滤波结构预留

### 13.6 参数窗口

- 幅值窗口
- 状态窗口
- 时基窗口
- 系统信息窗口
- 设置窗口

从软件架构角度看，它已经把“示波器 UI”拆成了主显示区和多个辅助功能面板。

## 14. 当前源码里需要特别注意的地方

### 14.1 主波形显示现在带有演示占位逻辑

`_DrawWave()` 里目前用了 `rand()` 来生成显示数据。

这说明当前工程更像是：

- 绘制架构可用
- 数据路径半演示状态

所以后续做正式示波器应用时，显示结构可以直接学，但数据接入要用你自己的采样数据替换。

### 14.2 真实数据准备和显示层是分开的

从工程设计角度看，作者原本想要的分层是：

1. `DSO_WaveProcess.c`
   负责把 ADC 数据处理成可显示样本。

2. `_DrawWave()`
   负责把“已经准备好的样本”画出来。

这个分层是对的，后面我们也应该保留。

### 14.3 一些窗口和功能存在“预留但未完全启用”的痕迹

例如：

- FFT 绘制代码大量注释
- 某些状态窗口的定时更新代码注释较多
- 某些对话框创建被注释

所以这个例程更像“完整产品雏形 + 部分功能裁剪版”，不是每个模块都完全处于最终交付状态。

## 15. 迁移到 AppWizard slot 时应该怎么理解

你后面要把示波器应用画到之前的 `slot` 文件里，而且要遵守 AppWizard 的调用风格。这里先不写代码，只讲结构。

### 15.1 最应该保留的，不是某一行代码，而是分层

后续迁移时建议保留下面这个结构：

1. AppWizard 屏幕层
   负责页面切换、基础控件布局、BOX 容器、按钮等。

2. 示波器静态框架层
   负责边框、标题、固定刻度说明、状态标签。

3. 示波器动态波形层
   负责网格、波形、游标、触发箭头。

4. 交互层
   负责触摸拖动、缩放、游标移动等。

### 15.2 AppWizard 里最适合放示波器的地方

结合你前面已经调通过的经验，后续最适合的方式不是在整个 screen 上乱画，而是：

- 在 `ID_SCREEN_01` 里的某个 `BOX` 或专门的自定义区域上画

原因是：

- AppWizard 管理的 screen 自己有控件层级
- 直接在整个 screen 根窗口上随意画，容易被其他控件覆盖
- 画到指定 `BOX` 的回调里，更符合 AppWizard 的管理方式

### 15.3 对应关系怎么映射

可以这样理解：

- DSO 例程里的 `DSO_Init()`
  对应 AppWizard 页面进入后的静态框架初始化

- DSO 例程里的 `DSO_DrawBakFrame()`
  对应 AppWizard 里示波器区域的网格背景绘制

- DSO 例程里的 `_DrawWave()`
  对应 AppWizard 的自定义绘制回调中，波形区的动态绘制

- DSO 例程里的 `_cbMotion()`
  对应 AppWizard 页面里用于交互的触摸逻辑或自定义消息处理

### 15.4 在 AppWizard 风格下，后续建议的职责拆分

后续真正写代码时，建议拆成下面几类函数：

1. 初始化函数
   只做一次性的资源创建、内存设备创建、默认参数设置。

2. 静态绘制函数
   只负责画不会频繁变化的内容。

3. 动态绘制函数
   每次刷新时画波形、游标、触发等。

4. 数据准备函数
   只管把采样数据转换成屏幕点，不直接操作 AppWizard 控件。

5. 事件处理函数
   只处理按钮、滑动、触摸，不直接在事件里塞大量绘图。

这个拆法和当前 DSO 例程的思想是完全一致的。

## 16. 后续真正落到 `slot` 文件时的建议顺序

等下一步开始写代码时，建议按这个顺序做，而不是一次性全塞进去：

1. 先在 AppWizard 的 `BOX` 上复现网格背景
2. 再在同一绘制区域里复现单通道波形
3. 再加触发箭头
4. 再加游标
5. 再加顶部状态栏
6. 最后再接真实采样数据与交互逻辑

这样做最稳，因为每一步都能单独验证坐标和层次是否正确。

## 17. 最终结论

这个 emWin DSO 例程的本质，不是一个“Graph 控件示例”，而是一个“示波器自绘框架示例”。

它最值得学习的不是某个单独 API，而是下面这套组合：

1. 用 `GUI_MEMDEV` 缓存静态背景
2. 用 `GUI_DrawGraph` 画动态波形
3. 用 `GUI_FillPolygon` / `GUI_DrawPixel` / `GUI_DrawRect` 手工补充示波器专属元素
4. 用透明窗口承接交互
5. 把数据准备与绘制分层

后续要搬到 AppWizard 时，最正确的方向也是：

- 保留这套分层思想
- 把“主波形区”作为一个自绘区域放进 `slot`
- 不要把整个示波器问题误解成“堆几个标准控件”

如果下一步继续做代码，我建议先从“在 `ID_SCREEN_01_Slots.c` 对应的 BOX 区域里，按 AppWizard 风格画出网格和单通道波形”开始，这是最稳的第一步。
