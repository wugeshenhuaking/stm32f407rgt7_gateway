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

**能不能局部绘制**
可以，完全可以，而且 DSO 原工程就是这么干的。

你看 DSO 原工程在拖动参考位置的时候，不是整窗刷新，而是：

- [MainTask.c:1256](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/TommyRIckWu/.vscode/extensions/openai.chatgpt-26.325.31654-win32-x64/webview/) WM_InvalidateArea(&rRefPos);

也就是只让“参考位置相关区域”重绘，不让整屏都重画。
同样，拖动顶部浏览条的时候它只刷：

- [MainTask.c:1289](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/TommyRIckWu/.vscode/extensions/openai.chatgpt-26.325.31654-win32-x64/webview/) WM_InvalidateArea(&rTrigPos);

所以答案是：

- 能局部绘制
- 你现在闪，是因为我这版还没把刷新区域细化

## 18. **别人的 DSO 工程有没有开缓冲，怎么看**

你可以按这几个层次去查。

**1. 先看有没有开“窗口内存设备”**
搜这个：

- WM_SetCreateFlags(WM_CF_MEMDEV)

在 DSO 原工程里有：

- [MainTask.c:1405](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/TommyRIckWu/.vscode/extensions/openai.chatgpt-26.325.31654-win32-x64/webview/)

这表示窗口绘制会优先走 emWin 的窗口内存设备，能明显减轻闪烁。

**2. 再看有没有开“多缓冲”**
搜这个：

- WM_MULTIBUF_Enable(1)

在 DSO 的 MainTask.c 里，这个函数名字出现在注释说明里：

- [MainTask.c:1417](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/TommyRIckWu/.vscode/extensions/openai.chatgpt-26.325.31654-win32-x64/webview/)

它注释里明确说了两件事：

- WM_MULTIBUF_Enable(1) 是多缓冲
- WM_SetCreateFlags(WM_CF_MEMDEV) 是窗口内存设备
- 如果都支持，优先多缓冲

注意这里你要分清：

- 代码里“提到这个函数”不代表一定启用了
- 要看它是不是实际被调用了
- 还要看 LCDConf.c 是否真的支持多缓冲

**3. 再看有没有开“自定义内存设备缓存”**
搜：

- GUI_MEMDEV_CreateFixed
- GUI_MEMDEV_Select
- GUI_MEMDEV_WriteAt

DSO 原工程明确用了两块：

- [MainTask.c:1437](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/TommyRIckWu/.vscode/extensions/openai.chatgpt-26.325.31654-win32-x64/webview/)
- [MainTask.c:1445](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/TommyRIckWu/.vscode/extensions/openai.chatgpt-26.325.31654-win32-x64/webview/)

它的意思是：

- 静态网格背景先画进 memdev
- 动态波形再画进另一个 memdev
- 最后一次性贴到屏幕

这也是它不容易闪的重要原因。

**4. 最关键，再去看 LCDConf.c 是否真的支持多缓冲**
你要搜这些关键词：

- NUM_BUFFERS
- LCD_X_SHOWBUFFER
- LCD_DEVFUNC_COPYBUFFER
- LCD_DEVFUNC_MULTIBUF
- COPYBUFFER
- SHOWBUFFER

你当前工程的 [LCDConf.c](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/TommyRIckWu/.vscode/extensions/openai.chatgpt-26.325.31654-win32-x64/webview/) 里，我看到的是：

- 有 LCD_X_SHOWBUFFER 这个 case，但只是空壳返回 0
- 没看到 NUM_BUFFERS
- 没看到真正的多缓冲切换实现
- 没看到帧缓冲数组或 buffer index 管理

这基本说明你当前这个工程的 LCD 层，大概率**没有真正把硬件/驱动级多缓冲跑起来**。

**你当前工程现在是什么状态**
可以这样理解：

**有的**

- emWin 动态内存池，在 [GUIConf.c](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/TommyRIckWu/.vscode/extensions/openai.chatgpt-26.325.31654-win32-x64/webview/) 这一套
- DSO 原工程里有 WM_CF_MEMDEV 和 GUI_MEMDEV 的使用范例
- 你当前 AppWizard 页面已经有单独的波形触摸子窗口

**没有真正用上的**

- 你当前这个 AppWizard 页面还没有做局部失效刷新
- 也没有给这个页面单独做波形 memdev
- 从当前 LCDConf.c 看，也不像已经真正启用了硬件多缓冲

**所以你现在看到闪烁的直接原因**
不是“有没有缓冲”这么抽象，而是两个更直接的点：

1. 我现在每次拖动都在 WM_InvalidateWindow(hParent)，导致整页重画。
2. 整页重画这套自绘内容还没有挂到局部缓存上。

**你去别人 DSO 工程里重点看什么**
你去看时，按这个顺序查最省时间：

1. 搜 WM_InvalidateArea
   看它拖动时是局部刷新还是整窗刷新。
2. 搜 WM_SetCreateFlags(WM_CF_MEMDEV)
   看它有没有开窗口内存设备。
3. 搜 GUI_MEMDEV_CreateFixed
   看它有没有自己给波形区做缓存。
4. 看 LCDConf.c
   搜 LCD_X_SHOWBUFFER、NUM_BUFFERS、COPYBUFFER
   确认是不是驱动层真的支持多缓冲。

好问题！让我为您解释 EMWIN 中的**脏矩阵（Dirty Rectangle）**概念。

## 19. 脏矩阵的定义

**脏矩阵**是指屏幕上需要重新绘制的矩形区域。在 EMWIN 中：

- 当窗口或控件的内容发生变化时，该区域被标记为"脏"（dirty）
- EMWIN 只会重新绘制这些被标记为脏的区域，而不是整个屏幕
- 这是一种优化技术，可以提高绘图效率

**工作原理**

Code

```
未修改区域 → 无需重绘
已修改区域 → 标记为脏 → 需要重绘
未修改区域 → 无需重绘
```

非常好的深层次问题！让我为您详细解释 EMWIN 的脏矩阵机制和内存管理。

### 内存布局

您的理解基本正确。对于 800×480 的单缓冲，16 位色深：

```
显存/内存: 800 × 480 × 2 bytes = 768 KB
```

这块内存直接对应屏幕的每个像素。

### 脏矩阵的标记机制

EMWIN **确实隐藏了底层实现**，但原理如下：

```c
// EMWIN 内部大概这样维护脏矩阵
typedef struct {
    int x0, y0;      // 脏矩形的左上角
    int x1, y1;      // 脏矩形的右下角
} GUI_RECT;

// 全局脏矩形列表（EMWIN 内部维护）
static GUI_RECT _aDirtyRects[MAX_DIRTY_RECTS];
static int _NumDirtyRects = 0;
```

### 标记过程

当您调用 `GUI_InvalidateRect()` 时：

```c
void GUI_InvalidateRect(GUI_RECT *pRect) {
    // 1. 接收需要重绘的矩形
    // 2. 添加到脏矩形列表中
    _aDirtyRects[_NumDirtyRects++] = *pRect;
    
    // 3. 可能进行矩形合并优化（避免矩形过多）
    _MergeOverlappingRects();
}
```

### 重绘过程

在 `GUI_Exec()` 或事件循环中：

```c
void GUI_Exec(void) {
    // 遍历所有脏矩形
    for(int i = 0; i < _NumDirtyRects; i++) {
        GUI_RECT *pDirtyRect = &_aDirtyRects[i];
        
        // 只重绘这个矩形区域内的窗口
        _RedrawWindowsInRect(pDirtyRect);
        
        // 只清除这个矩形区域内的显存
        // 实际上是逐行写入像素数据
        // 对于 (x0,y0) 到 (x1,y1) 的矩形：
        for(int y = pDirtyRect->y0; y <= pDirtyRect->y1; y++) {
            // 计算显存地址
            uint16_t *pPixel = (uint16_t *)pFrameBuffer 
                              + y * 800 + pDirtyRect->x0;
            
            // 写入宽度 = x1 - x0 + 1 个像素
            memcpy(pPixel, pDrawBuffer, 
                   (pDirtyRect->x1 - pDirtyRect->x0 + 1) * 2);
        }
    }
    
    // 清空脏矩形列表
    _NumDirtyRects = 0;
}
```

### 具体数据流示例

假设您的界面是 800×480，单缓冲，16 位色深：

```
屏幕坐标系:
0,0 ─────────────────────── 800,0
│                              │
│  [脏矩形1]                   │
│  (100,100)─(200,200)        │
│                              │
│        [脏矩形2]             │
│        (300,50)─(400,100)   ���
│                              │
└─────────────────────── 800,480


显存地址计算:
像素 (x, y) 的地址 = pFrameBuffer + (y * 800 + x) * 2

脏矩形1 (100,100)-(200,200):
  起始像素: y=100, x=100 → offset = (100*800 + 100)*2 = 160200 bytes
  宽度: 200-100+1 = 101 pixels = 202 bytes
  高度: 200-100+1 = 101 lines
  每次处理一行，共处理 101 行
  总字节数: 101 * 202 = 20402 bytes

脏矩形2 (300,50)-(400,100):
  起始像素: y=50, x=300 → offset = (50*800 + 300)*2 = 80600 bytes
  宽度: 400-300+1 = 101 pixels = 202 bytes
  高度: 100-50+1 = 51 lines
  总字节数: 51 * 202 = 10302 bytes
```

### EMWIN 的实现细节

EMWIN 的脏矩阵机制大概如下（伪代码）：

```c
// 当您调用 GUI_InvalidateRect(pRect) 时
{
    // EMWIN 内部的脏矩形管理
    extern GUI_RECT _ClipRect;
    extern GUI_RECT _aInvalidRects[GUI_NUM_LAYERS][MAX_INVALID_RECTS];
    extern int _NumInvalidRects[GUI_NUM_LAYERS];
    
    // 添加到脏矩形列表
    _aInvalidRects[_CurrentLayer][_NumInvalidRects[_CurrentLayer]++] = *pRect;
}

// 在主循环中
{
    while(1) {
        // 处理窗口消息
        WM_Exec();
        
        // 检查是否有脏矩形
        if(_NumInvalidRects[_CurrentLayer] > 0) {
            // 对每个脏矩形，发送 WM_PAINT 消息给相关窗口
            for(int i = 0; i < _NumInvalidRects[_CurrentLayer]; i++) {
                WM_SendMessage(hWin, &PaintMsg);
            }
            
            // 清空脏矩形列表
            _NumInvalidRects[_CurrentLayer] = 0;
        }
    }
}
```

### 关键点总结

| 方面           | 说明                                             |
| -------------- | ------------------------------------------------ |
| **内存**       | 确实是 800×480×2 的显存 + EMWIN 内部的脏矩形列表 |
| **脏矩形存储** | EMWIN 维护一个矩形数组，记录哪些区域需要重绘     |
| **标记方式**   | 调用 `GUI_InvalidateRect()` 添加到脏矩形列表     |
| **重绘触发**   | 在 `GUI_Exec()` 或事件循环中处理脏矩形           |
| **优化**       | 只更新脏矩形区域的像素，避免全屏重绘             |
| **隐藏实现**   | EMWIN 把这些细节封装起来，用户只需调用接口       |

### 回到您最初的问题

您在 BOX 上直接调用 EMWIN 库函数看不到显示，原因就是：
- **该区域没有被标记为脏矩形**
- EMWIN 的重绘循环不知道需要重画这个区域
- 解决方案：调用 `GUI_InvalidateRect()` 显式标记需要重绘的区域

非常深刻的理解！您这两个问题触及了示波器的核心工作原理。让我为您详细解释。

## 问题1：示波器的采集和显示是否实时

### 您的理解是完全正确的！

```
真实信号时间轴:
|----采集点1----|----采集点2----|----采集点3----|----采集点4----|
时间: t1        t2             t3             t4

示波器显示时间轴（滞后）:
                |----显示采集点1----|----显示采集点2----|
                处理延迟            显示延迟

现实: 信号已经采完，处理完，显示出来时，真实的信号早就过去了
```

### 采集流程（典型示波器）

```c
// 实时采集线程/中断处理
void ADC_ISR(void) {
    // 1. 采集点被记录到缓冲区（这时候信号已经发生了）
    uint16_t adcValue = ADC_ReadValue();
    
    // 2. 保存到环形缓冲区
    circularBuffer[writeIndex++] = adcValue;
    if(writeIndex >= BUFFER_SIZE) writeIndex = 0;
    
    // 这时候信号已经过去了！
    // 中断处理完毕，返回
}

// 显示线程（CPU 处理的过程）
void Display_Thread(void) {
    while(1) {
        // 1. 从缓冲区读取已采集的数据
        for(int i = 0; i < DISPLAY_POINTS; i++) {
            displayBuffer[i] = circularBuffer[readIndex++];
        }
        
        // 2. 处理数据（这会花时间）
        ProcessData(displayBuffer);
        
        // 3. 绘制波形（这也会花时间）
        DrawWaveform(displayBuffer);
        
        // 4. 更新屏幕（这还要花时间）
        GUI_Invalidate();
        
        // 全部完成后，屏幕才显示这些已经采集好很久的数据
    }
}
```

### 实际的时间延迟

```
对于一个典型的数字示波器（采样率 100 kHz，屏幕 800 像素）:

采集延迟:
  从模拟信号到 ADC 转换: 微秒级 (ns ~ μs)
  信号已过去时间: 几微秒

处理延迟（这是主要的）:
  采集 800 个点: 800 / 100kHz = 8 ms
  数据处理: 1-5 ms
  GPU/屏幕刷新: 16-33 ms (60Hz 刷新率)
  总延迟: 25-50 ms
  
最终结果: 屏幕上显示的波形是 25-50 ms 之前发生的信号！

对于高速示波器（采样率 GHz 级）:
  延迟会更大，因为数据处理需要更长时间
```

### 您说的"像是实时"的原因

```
为什么看起来像实时?

1. 人眼刷新率限制
   └─ 人眼对 60Hz 以上的刷新率无法分辨
   
2. 信号本身是周期性的
   └─ 示波器显示的是一个完整周期（或多个周期）
   └─ 即使数据滞后 50ms，显示的波形仍然是正确的形状
   
3. 连续采集和显示
   ┌────────────────┬────────────────┬────────────────┐
   │ 采集周期1      │ 采集周期2      │ 采集周期3      │
   └────────────────┴────────────────┴────────────────┘
      │显示周期1      │显示周期2      │显示周期3
      
   虽然显示延迟，但因为信号是连续的，看起来像实时流动

4. CPU 足够快
   └─ 处理和刷新速度快，人眼感觉不到明显延迟
```

### 不同类型示波器的区别

| 类型           | 延迟           | 原因                              |
| -------------- | -------------- | --------------------------------- |
| **模拟示波器** | 极低（微秒级） | 直接在 CRT 屏幕上画，无需数字处理 |
| **数字示波器** | 中等（毫秒级） | 需要采集、处理、显示的完整流程    |
| **实时示波器** | 低（几毫秒）   | 优化的处理管线，但仍有延迟        |
| **采样示波器** | 相对较低       | 对周期信号的多周期采样            |

---

## 问题2：滚动更新波形 vs 普通更新

这是一个很好的优化问题！两者有很大区别。

### 普通更新（全屏重绘）

```
显示更新流程（每帧）:

第1帧:
┌──────────────────────────┐
│ [旧波形] [旧数据] [旧数据] │
└──────────────────────────┘

第2帧:
1. 清除整个屏幕 (时间 T1)
2. 重新绘制整条波形线 (时间 T2)
3. 重新绘制所有像素 (时间 T3)
4. 刷新整个屏幕 (时间 T4)

┌──────────────────────────┐
│ [新波形] [新数据] [新数据] │
└──────────────────────────┘

时间成本: T1 + T2 + T3 + T4 = 很大！
```

### 滚动更新（卷动显示）

```
显示更新流程（每帧）:

原理：把屏幕当作一个环形缓冲区，只更新新增的部分

第1帧（初始状态）:
┌──────────────────────────┐
│ [数据1] [数据2] [数据3]...│ [数据N]
└──────────────────────────┘

第2帧（新数据到达）:
┌──────────────────────────────────┐
│ [数据2] [数据3] [数据4]...│ [数据N] [新数据N+1]
└─��────────────────────────────────┘

实现方式1：物理滚动（最高效）
  - 显存地址偏移，不移动像素数据
  - 只在右边绘制新增的一列像素
  - 时间成本: 仅绘制新增数据
  
实现方式2：逻辑滚动
  - 内存中的数据向左移位
  - 新数据追加到右边
  - 时间成本: 稍高，但比全屏重绘好得多
```

### 具体代码对比

#### ❌ 普通更新（低效）

```c
void Update_Waveform_Normal(void) {
    // 问题：每次都要重绘整个屏幕
    
    // 1. 清屏（很费时！）
    GUI_SetBkColor(GUI_BLACK);
    GUI_Clear();  // 清除整个 800×480
    
    // 2. 重新绘制所有波形点（很费时！）
    for(int i = 0; i < 800; i++) {
        int y = waveformData[i];
        GUI_SetColor(GUI_GREEN);
        GUI_DrawPixel(i, y);
    }
    
    // 3. 刷新整个屏幕
    GUI_Invalidate();
}

时间成本分析:
  清屏: 800 × 480 = 384,000 像素
  绘制: 800 个点的连线
  总耗时: ~33 ms（可能超过一帧时间 16.67 ms@60Hz）
  结果: 波形卡顿或闪烁
```

#### ✅ 滚动更新（高效）

```c
void Update_Waveform_Scroll(void) {
    // 方式1：硬件滚动（最优，如果 LCD 支持）
    // 修改 LCD 的起始地址寄存器
    int newStartAddr = pFrameBuffer + (scrollOffset * 2);
    LCD_SetFrameBufferAddr(newStartAddr);
    
    // 这样做的效果（逻辑上）:
    // ┌──────────────────────────┐
    // │ [2] [3] [4] [5] ... [N] [N+1]
    // └──────────────────────────┘
    //  ^原来的[1]现在看不见了，[N+1]是新增数据
    
    // 时间成本: 仅改变一个地址，微秒级！
    
    // 方式2：软件滚动（没有硬件支持时）
    // 1. 只绘制右侧新增的列（而不是整个屏幕）
    int newDataX = 799;  // 屏幕最右边
    int newDataValue = waveformData[newestIndex];
    
    // 只更新最右边一列像素
    GUI_SetColor(GUI_GREEN);
    for(int y = 0; y < 480; y++) {
        if(y == newDataValue) {
            GUI_DrawPixel(newDataX, y);
        } else {
            // 用黑色覆盖旧像素
            GUI_SetColor(GUI_BLACK);
            GUI_DrawPixel(newDataX, y);
            GUI_SetColor(GUI_GREEN);
        }
    }
    
    // 标记只需更新这一列的矩形区域
    GUI_Rect_t updateRect = {newDataX, 0, newDataX, 479};
    GUI_InvalidateRect(&updateRect);  // 关键！只更新这一小部分
    
    时间成本: 仅 480 个像素，远小于全屏 384,000 像素！
}

时间成本对比:
  普通更新: 800 × 480 = 384,000 像素
  滚动更新:       480 = 480 像素
  性能提升: 约 800 倍！
```

### 实际示波器中的滚动更新

```c
// 环形缓冲区 + 滚动显示（真实场景）

#define BUFFER_SIZE 2000   // 存储 2000 个采样点
uint16_t dataBuffer[BUFFER_SIZE];
int writeIndex = 0;        // 写入指针
int displayStartIndex = 0; // 显示的起始指针

void ADC_ISR(void) {
    // 采集新数据
    dataBuffer[writeIndex] = ADC_ReadValue();
    writeIndex++;
    if(writeIndex >= BUFFER_SIZE) writeIndex = 0;
}

void Display_Thread(void) {
    while(1) {
        // 只绘制新增的点（向右卷动）
        int newDataIndex = (displayStartIndex + 799) % BUFFER_SIZE;
        int y = ConvertADCToPixelY(dataBuffer[newDataIndex]);
        
        // 仅在屏幕右边绘制新增的一个点
        GUI_SetColor(GUI_GREEN);
        GUI_DrawPixel(799, y);
        
        // 标记只需更新右边这一列
        GUI_Rect_t rect = {799, 0, 799, 479};
        GUI_InvalidateRect(&rect);
        
        // 移动窗口指针，造成卷动效果
        displayStartIndex++;
        if(displayStartIndex >= BUFFER_SIZE) 
            displayStartIndex = 0;
        
        sleep(10);  // 每 10ms 更新一次
    }
}
```

## 总结对比表

| 方面               | 普通更新          | 滚动更新               |
| ------------------ | ----------------- | ---------------------- |
| **每帧更新像素数** | 整个屏幕（~384K） | 仅新增列（~480）       |
| **性能**           | 低（可能掉帧）    | 高（流畅）             |
| **CPU 占用**       | 高                | 低                     |
| **视觉效果**       | 闪烁、卡顿        | 流畅卷动               |
| **实现难度**       | 简单              | 中等（需管理缓冲区）   |
| **适用场景**       | 简单仪表盘        | **示波器、实时曲线图** |

---

## 您对示波器工作原理的理解

您的观察非常正确：
1. ✅ 采集是实时的，但显示确实有延迟
2. ✅ 延迟主要来自数据处理和屏幕刷新
3. ✅ 看起来实时是因为 CPU 快 + 人眼无法分辨
4. ✅ 滚动更新比普通更新高效得多

这就是为什么真正的示波器和实时曲线图应用都采用**滚动更新机制**！
