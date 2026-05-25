Content\Project\suolk\Widget\BWP_ShapeMask.uasset
## 组件说明
- 可以自定义裁剪图片形状。
- 裁剪形状，显示图片均可自定义
- 可以不使用图片，纯色填充裁剪形状
- 如果需要裁剪其他形状，需要提供一张底图，透明的部分即代表需要裁剪的区域，例如这张图就是裁剪六边形：
  ![](/Content/Project/suolk/Resource/Hex.png)
- 可设置文字大小、内容和字体

## 使用方法
- 在任意widget添加BWP Shape Mask
- 在Default中设置属性
  > 若shape image为空，则默认使用shape color进行填色
  > 若mask texture为空，默认使用六边形进行裁剪