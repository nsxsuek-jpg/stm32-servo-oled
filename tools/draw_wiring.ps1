# 生成 docs/wiring.png 接线图（Windows PowerShell 5.1 / 7 均可）
# 用法： powershell -ExecutionPolicy Bypass -File tools\draw_wiring.ps1
# 想改引脚名或文字，直接改下面的 $rows / $powRows 数组再跑一次即可。
# 脚本末尾会做一次文字宽度自检，确保没有文字溢出框体。
Add-Type -AssemblyName System.Drawing

$out = Join-Path $PSScriptRoot '..\docs\wiring.png'   # 相对仓库根目录
$W = 1180; $H = 1000; $S = 2

$bmp = New-Object System.Drawing.Bitmap ([int]($W*$S)), ([int]($H*$S))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
$g.ScaleTransform($S, $S)
$g.Clear([System.Drawing.Color]::FromArgb(248, 250, 252))

function Col([string]$h) { [System.Drawing.ColorTranslator]::FromHtml($h) }
function MkFont([single]$sz, [string]$fam, [bool]$bold) {
  $st = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
  New-Object System.Drawing.Font($fam, $sz, $st, [System.Drawing.GraphicsUnit]::Pixel)
}
function RRect([single]$x,[single]$y,[single]$w,[single]$h,[single]$r) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = $r*2
  $p.AddArc($x, $y, $d, $d, 180, 90)
  $p.AddArc($x+$w-$d, $y, $d, $d, 270, 90)
  $p.AddArc($x+$w-$d, $y+$h-$d, $d, $d, 0, 90)
  $p.AddArc($x, $y+$h-$d, $d, $d, 90, 90)
  $p.CloseFigure()
  return $p
}
$warn = New-Object System.Collections.ArrayList
function Chk([string]$txt,$font,$maxW,[string]$tag) {
  $sz = $g.MeasureString($txt, $font)
  if ($sz.Width -gt $maxW) { [void]$warn.Add(("{0}: '{1}' 宽 {2:N0}px > {3:N0}px" -f $tag,$txt,$sz.Width,$maxW)) }
}

$fTitle = MkFont 27 'Microsoft YaHei' $true
$fSub   = MkFont 14 'Microsoft YaHei' $false
$fBoard = MkFont 19 'Microsoft YaHei' $true
$fBoardS= MkFont 13 'Microsoft YaHei' $false
$fPin   = MkFont 15 'Consolas' $true
$fPinS  = MkFont 12 'Consolas' $true
$fBody  = MkFont 15 'Microsoft YaHei' $false
$fNote  = MkFont 13 'Microsoft YaHei' $false
$fLeg   = MkFont 14 'Microsoft YaHei' $false

$bText  = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(17,24,39))
$bSub   = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(100,116,139))
$bWhite = [System.Drawing.Brushes]::White
$bNote  = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(71,85,105))

$sfNear = New-Object System.Drawing.StringFormat
$sfNear.Alignment = [System.Drawing.StringAlignment]::Near
$sfNear.LineAlignment = [System.Drawing.StringAlignment]::Center
$sfFar = New-Object System.Drawing.StringFormat
$sfFar.Alignment = [System.Drawing.StringAlignment]::Far
$sfFar.LineAlignment = [System.Drawing.StringAlignment]::Center
$sfMid = New-Object System.Drawing.StringFormat
$sfMid.Alignment = [System.Drawing.StringAlignment]::Center
$sfMid.LineAlignment = [System.Drawing.StringAlignment]::Center

# ---------- 标题 ----------
$title = 'STM32F103C8T6 舵机控制仪 · 接线图'
$g.DrawString($title, $fTitle, $bText, 40, 34)
$sub = 'Blue Pill (72MHz) + SSD1306 OLED + MG90S 舵机 + 10k 电位器 + 3 按键 + 蜂鸣器 + 3 个指示 LED'
$g.DrawString($sub, $fSub, $bSub, 42, 74)

# ---------- 数据 ----------
$rows = @(
 @{p='PA0';      c='#2563EB'; t='电位器 10k · 中间抽头';            n='ADC_IN0'},
 @{p='PA1';      c='#2563EB'; t='按键「左」一端';                    n='内部上拉，按下接 GND'},
 @{p='PA2';      c='#2563EB'; t='按键「右」一端';                    n='同上'},
 @{p='PA3';      c='#2563EB'; t='按键「翻页」一端';                  n='同上'},
 @{p='PA6';      c='#7C3AED'; t='OLED  SDA';                         n='软件 I2C'},
 @{p='PA7';      c='#7C3AED'; t='OLED  SCL';                         n='开漏输出'},
 @{p='PB0';      c='#0D9488'; t='蜂鸣器  I/O';                       n='默认低电平触发'},
 @{p='PB1';      c='#CA8A04'; t='LED1（指示第 1 页）';               n='串 1k 限流电阻'},
 @{p='PB12';     c='#CA8A04'; t='LED2（指示第 2 页）';               n='同上'},
 @{p='PB13';     c='#CA8A04'; t='LED3（指示第 3 页）';               n='同上'},
 @{p='PB6';      c='#DC2626'; t='舵机 信号线（橙/黄/白）';           n='TIM4_CH1  50Hz'},
 @{p='PB7/8/9';  c='#DC2626'; t='舵机信号线（备用，输出完全相同）';  n='TIM4_CH2/3/4'}
)

$powRows = @(
 @{p='5V';  c='#B91C1C'; t='舵机 VCC（红）· OLED 与蜂鸣器的 VCC';        n='舵机必须用 5V'},
 @{p='3V3'; c='#EA580C'; t='电位器 另一端';                              n='3.3V'},
 @{p='GND'; c='#111827'; t='舵机 GND（棕/黑）· OLED · 蜂鸣器 · 按键公共端'; n='★ 必须共地'}
)

# ---------- 板子 ----------
$bdX = 40; $bdY = 108; $bdW = 240
$rowY0 = 216; $dy = 38
$rowN = $rows.Count + $powRows.Count + 1   # +1 为中间的分隔行
$bdH = ($rowN * $dy) + 46
$bp = RRect $bdX $bdY $bdW $bdH 12
$bBoard = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(15,52,84))
$g.FillPath($bBoard, $bp)
$g.DrawString('STM32F103C8T6', $fBoard, $bWhite, (New-Object System.Drawing.RectangleF($bdX, ($bdY+14), $bdW, 26)), $sfMid)
$g.DrawString('Blue Pill · 72MHz', $fBoardS, (New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(148,180,210))), (New-Object System.Drawing.RectangleF($bdX, ($bdY+42), $bdW, 20)), $sfMid)

$wireX0 = $bdX + $bdW + 6
$wireX1 = 556
$chipX  = 570; $chipW = 392; $chipH = 28
$noteX  = 976; $noteW = 190

$y = $rowY0
$allRows = @()
foreach ($r in $rows) { $allRows += ,@{ kind='pin'; d=$r } }
$allRows += ,@{ kind='sep'; d=$null }
foreach ($r in $powRows) { $allRows += ,@{ kind='pin'; d=$r } }

foreach ($item in $allRows) {
  if ($item.kind -eq 'sep') {
    $penSep = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(203,213,225)), 1
    $penSep.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
    $g.DrawLine($penSep, $bdX, $y, 1160, $y)
    $g.DrawString('电源 / 地', $fNote, $bSub, 60, ($y - 9))
    $y += $dy
    continue
  }
  $d = $item.d
  $pen = New-Object System.Drawing.Pen ((Col $d.c)), 2.6
  $br  = New-Object System.Drawing.SolidBrush ((Col $d.c))
  $g.DrawLine($pen, $wireX0, $y, $wireX1, $y)
  $g.FillEllipse($br, ($wireX0-5), ($y-5), 10, 10)

  $pf = if ($d.p.Length -gt 8) { $fPinS } else { $fPin }
  Chk $d.p $pf ($bdW-28) ('pin ' + $d.p)
  $g.DrawString($d.p, $pf, $bWhite, (New-Object System.Drawing.RectangleF(($bdX+8), ($y-14), ($bdW-24), 28)), $sfFar)

  $cp = RRect $chipX ($y-$chipH/2) $chipW $chipH 8
  $fill = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,255,255))
  $g.FillPath($fill, $cp)
  $g.DrawPath($pen, $cp)
  Chk $d.t $fBody ($chipW-24) ('target ' + $d.p)
  $g.DrawString($d.t, $fBody, $bText, (New-Object System.Drawing.RectangleF(($chipX+12), ($y-14), ($chipW-24), 28)), $sfNear)

  if ($d.n) {
    Chk $d.n $fNote $noteW ('note ' + $d.p)
    $g.DrawString($d.n, $fNote, $bNote, (New-Object System.Drawing.RectangleF($noteX, ($y-13), $noteW, 26)), $sfNear)
  }
  $y += $dy
}

# ---------- 图例 ----------
$ly = $y + 16
$lp = RRect 40 $ly 1120 132 12
$g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255,247,237))), $lp)
$g.DrawPath((New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(253,186,116)), 1.5), $lp)
$leg = @(
 '· 舵机三根线：棕/黑 = GND，红 = VCC（5V），橙/黄/白 = 信号',
 '· 按键：一端接引脚，另一端接 GND（芯片内部上拉，不用外接电阻）',
 '· 电位器：两端分别接 3V3 与 GND，中间抽头接 PA0',
 '· 指示 LED：正极串 1k 电阻接引脚，负极接 GND',
 '· ★ 舵机的电源地必须和开发板共地，否则舵机完全不动；舵机堵转电流可达 1A，建议单独供 5V'
)
$lyy = $ly + 16
foreach ($t in $leg) {
  $g.DrawString($t, $fLeg, $bText, 60, $lyy)
  $lyy += 22
}

$g.Dispose()
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Output ('saved: ' + $out)
Write-Output ('size : ' + [math]::Round((Get-Item $out).Length/1KB) + ' KB  (' + $W + 'x' + $H + ' logical, ' + ($W*$S) + 'x' + ($H*$S) + ' px)')
if ($warn.Count -eq 0) { Write-Output 'overlap check: 全部文本都在框内 OK' }
else { Write-Output 'overlap check:'; $warn | ForEach-Object { '  ' + $_ } }
