# 项目 3D 模型与来源

正式工程使用原厂或立创原库 STEP，已移除此前生成的简化参考模型。原始下载文件保留原字节；Git 属性关闭这些 STEP 文件的换行转换。

| 文件 | 绑定器件 | 来源与处理 | KiCad 模型偏移 XYZ，mm |
| --- | --- | --- | --- |
| `ASE.step` | Y1、Y2 | Abracon ASE 原厂 STEP，未修改文件 | 0, 0, 0.5501 |
| `MIC-TH_BD4.0-P1.40-D0.4-L-FD.step` | MIC1 | 当前封装关联的立创原库 STEP，未修改文件 | 0, 0.55, 0 |
| `AUDIO-TH_PJ-3136-B_5pin.step` | CN2 | 立创原库模型适配现有五脚封装，仅去除多余的一根板下插脚 | -0.65, 0, 0 |
| `AUDIO-TH_PJ-3136-B_original_6pin.step` | 不绑定，保存来源 | 未修改的立创六脚原库 STEP | — |

全部绑定模型缩放为 1，模型旋转为 0。封装自身的板上旋转保持原值。偏移用于对齐原始 CAD 坐标与已有焊盘、定位孔及安装面；没有移动焊盘或修改 PCB。

## 原始文件

- Y1/Y2：[Abracon ASE 产品页](https://abracon.com/parametric/oscillators/ASE-12.500MHZ-E-T)、[原厂 STEP 压缩包](https://abracon.com/Support/STEP/Oscillators/ASE.STEP.zip)。同一 ASE 外形用于项目的 12 MHz、12.288 MHz 振荡器。模型含外壳、端子、ASE 字样及 1 脚标记。
- MIC1：当前封装的 `JLC_3DModel_Q` 指向模型组件 `8dd4f0cbe6e44287ae3997316d3d922d`。从[组件元数据](https://pro.lceda.cn/api/v2/components/8dd4f0cbe6e44287ae3997316d3d922d)读取 STEP 资源 `4acc112050c847b9bce81c581d36ca27`，[原库 STEP](https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/4acc112050c847b9bce81c581d36ca27)。BOM 对应 [GMI4015P-2C-42db，LCSC C233292](https://www.lcsc.com/product-detail/C233292.html)。这是原库文件，没有将其认定为麦克风厂商发布的 CAD。
- CN2：当前封装关联模型组件 `0aa59bf5c25f4b4fa5c1c64a30ebdee9`。从[组件元数据](https://pro.lceda.cn/api/v2/components/0aa59bf5c25f4b4fa5c1c64a30ebdee9)读取 STEP 资源 `0756711cf0d14f3998115dcd19f4102c`，[原库 STEP](https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/0756711cf0d14f3998115dcd19f4102c)。

## CN2 五脚适配

用户确认使用五脚版本，并要求按当前封装处理。现有封装包含五个金属插脚孔、两个定位孔；金属脚对应 2、3、4 三个电气端子，其中 2、4 各有两个孔。

取得的立创原库 CAD 和 [XKB PJ-3136-B 原厂 CAD](https://www.helloxkb.com/Home/Goods/goodsInfoxq/id/1628.html)均有六个金属插脚，不能直接完整绑定到现有封装。本项目从立创原库 CAD 仅切除安装面以下、模型坐标 X=0.5、Y=-3.15 mm 的多余中部插脚；保留外壳、内部触点、其余五根插脚及两个定位柱。导出时重设黑色塑料与金属显示颜色。适配文件不是原厂提供的五脚 CAD，未经修改的六脚原库文件另行保存。

适配脚本为 [`scripts/adapt_pj3136_5pin.py`](../../scripts/adapt_pj3136_5pin.py)，使用 FreeCAD 1.1.1 自带的 Python：

```powershell
& 'D:\FreeCAD 1.1\bin\python.exe' 'scripts/adapt_pj3136_5pin.py'
```

脚本检查原始文件 SHA-256、切除范围、实体有效性和五脚截面。KiCad 原生整板 STEP 导出后又检查了 MIC1 的两根引脚、CN2 的五根引脚与现有钻孔的对齐，以及 ASE、CN2 的安装高度。来源、哈希及检查记录见 [`original-model-sources.json`](../../reports/footprint-binding/2026-10-03/original-model-sources.json)、[`original-model-validation.json`](../../reports/footprint-binding/2026-10-03/original-model-validation.json)、[`five-pin-adaptation.json`](../../reports/footprint-binding/2026-10-03/five-pin-adaptation.json)。

模型用于 3D 预览与 STEP 装配导出。MIC1 保留原库 CAD 的外形和高度；CN2 的五脚适配只保证与当前封装对齐，精确的实物外形仍以所购版本的机械图为准。

模型通过 `${KIPRJMOD}/WM8978.3dshapes/` 相对路径引用。其余 50 个器件继续使用 KiCad 10 标准 3D 库。
