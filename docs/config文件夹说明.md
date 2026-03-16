说明：
1. 这个目录里的文件主要是示例与默认配置；程序运行时优先读取 `QtScrcpy.exe` 同级目录下的 `config` 文件夹。
2. 本地当前运行目录示例：`C:\develop\QtScrcpy-boost\output\x64\Release\config`
3. `userdata.ini` 由 `QSettings` 管理，不建议直接在里面穿插大量注释；详细说明统一放在这里。

[common]
更正：当前仓库默认示例路径为 `C:\develop\QtScrcpy-boost\output\x64\Release\config`；如果显式构建 `Debug`、`MinSizeRel` 或 `RelWithDebInfo`，最后一级目录会跟随对应配置名。

`userdata.ini` 的 `[common]` 继续存放通用启动配置，例如录屏路径、码率、窗口行为、RelativeLook 的公共默认值等。

示例：
```ini
[common]
MouseSmoothFrames=8
RelativeLookRawScale=30.0
```

普通鼠标兼容与设备独有板块
--------------------------------

这次新增的“设备独有板块”UI 板块，数据全部写入 `userdata.ini` 的设备分组 `[序列号]` 下。

这些配置：
1. 只对当前选中设备生效
2. 不要写在 `[common]`
3. 修改后会立即写盘
4. 运行中的普通鼠标路径会低频热重载，一般不需要重连设备
5. `relative view` 不受这一组参数影响

当前 UI 中的 `Mouse` 子区块对应这些键：

```ini
[设备序列号]
RemoteCursorEnabled=true
CursorSizePx=64
NormalMouseCompatEnabled=false
NormalMouseTouchPriorityEnabled=true
NormalMouseCursorThrottleEnabled=true
NormalMouseCursorFlushIntervalMs=33
NormalMouseCursorClickSuppressionMs=120
NormalMouseTapMinHoldMs=16
```

键说明
------

`RemoteCursorEnabled`
- 设备级远端黑色鼠标光标渲染开关。
- 优先读取 `[serial]` 下的值。
- 如果设备分组里没写，仍会回退到旧的 `config.ini -> [common] -> RemoteCursorEnabled`，保证老配置不突变。

`CursorSizePx`
- 设备级远端光标大小，范围 `8~128`。
- 同样优先读 `[serial]`，缺失时回退到旧的 `config.ini -> [common] -> CursorSizePx`。

`NormalMouseCompatEnabled`
- 非相对视角普通鼠标兼容总开关。
- `false` 或缺失时，普通鼠标兼容项保持关闭。

`NormalMouseTouchPriorityEnabled`
- `true` 表示普通触控消息高优先级，远端黑光标低优先级。

`NormalMouseCursorThrottleEnabled`
- `true` 表示普通模式远端黑光标启用限频与点击静默。
- `false` 表示黑光标恢复即时发送。

`NormalMouseCursorFlushIntervalMs`
- 远端黑光标刷新间隔，建议范围 `16~100`，默认建议 `33`。
- 数值越大，黑光标越稳，但越不跟手。

`NormalMouseCursorClickSuppressionMs`
- 点击前后黑光标静默窗口，建议范围 `0~300`，默认建议 `120`。
- 数值越大，普通点击越优先，但黑光标越容易出现短暂停顿。

`NormalMouseTapMinHoldMs`
- 普通左键轻点的最小按压时长，建议范围 `0~40`，默认建议 `16`。
- `0` 表示关闭这一层兼容。
- 数值越大，轻点更稳，但点击手感会稍慢。

什么时候调
----------

只在“非相对视角”的普通控制里出现下面问题时再调：
1. 普通点击时灵时不灵
2. 普通滑动、拖动偶发不响应
3. 黑色远端光标明显挤占普通点击

建议调节顺序：
1. 先打开 `NormalMouseCompatEnabled=true`
2. 如果还是偶发不响应，优先上调 `NormalMouseTapMinHoldMs`，每次建议加 `4ms`
3. 如果黑光标仍明显挤占点击，再调大 `NormalMouseCursorFlushIntervalMs` 或 `NormalMouseCursorClickSuppressionMs`
4. 如果点击已经稳定，但黑光标过慢，再反向逐步减小 `FlushIntervalMs / ClickSuppressionMs`

何时生效
--------

1. 在主界面“设备独有板块”里改动后，会立即写入 `userdata.ini`
2. 普通鼠标路径按低频热重载生效，通常不需要断开重连
3. 为了避免误判，保存后移动一下鼠标，或重新点击一次窗口，让下一轮输入触发读取
4. 如果关闭 `RemoteCursorEnabled`，运行中的远端黑光标会按现有隐藏语义清掉

全局配置与设备独有帧率
----------------------

主界面原来的“启动配置”板块已经更名为“全局配置”。

这次新增的“最大帧率”配置，不是 `show fps` 叠字开关，而是视频连接参数里的 `MaxFps` 上限：

```ini
[common]
MaxFps=75

[设备序列号]
MaxFps=120
```

规则如下：

1. `全局配置 -> 最大帧率` 对应 `userdata.ini -> [common] -> MaxFps`
2. `设备独有板块 -> 最大帧率` 只有勾选“启用独有配置”后，才会写入 `userdata.ini -> [serial] -> MaxFps`
3. 设备没有勾选独有配置时，不写 `[serial]/MaxFps`，运行时自动继承全局 `MaxFps`
4. `0` 表示不限制最大帧率
5. 如果 `userdata.ini [common]` 里没有 `MaxFps`，程序仍会兼容回退读取旧的 `config.ini -> [common] -> MaxFps`

何时生效：

1. 全局 `MaxFps` 改动后，会随现有全局配置保存流程写入 `userdata.ini`
2. 设备独有 `MaxFps` 勾选或改值后，会立即写入或清除对应 `[serial]/MaxFps`
3. `MaxFps` 不属于普通鼠标那种热更新项，必须在 `start server`、`restart server` 或重新连接设备后生效
4. `show fps` 仍然只控制窗口左上角 FPS 文本显示，不影响实际视频上限

Center Crop 设备独有化
----------------------

`center crop` 已从全局配置中移除，改成只在“设备独有板块”里配置。

规则如下：

1. `center crop` 现在只认 `userdata.ini -> [serial] -> VideoCenterCropSize`
2. 没有设备值时，默认关闭
3. 旧的 `userdata.ini -> [common] -> VideoCenterCropSize` 即使还留在文件里，这次之后也不再生效
4. 取消勾选设备独有板块里的 `center crop` 后，会删除对应 `[serial]/VideoCenterCropSize`
5. `crop size` 默认显示值为 `256`，只是 UI 默认值，不代表已经写入配置

示例：

```ini
[c45449e2]
VideoCenterCropSize=256
```

补充说明：

1. `VideoCenterCropFallbackWidth` 和 `VideoCenterCropFallbackHeight` 继续保留为隐藏配置，不进入 UI
2. `center crop` 的实际效果会在连接或重启服务后的新会话里按设备配置生效
3. `server.cpp` 和 `videoform.cpp` 现在都只按设备值读取，避免服务端和本地坐标映射出现不一致

开发者构建命令
--------------

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build --config Release
```
