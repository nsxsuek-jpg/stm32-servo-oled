# ============================================================
#  Generate docs/wiring.png  --  industrial-style wiring diagram
#  Windows PowerShell 5.1 / 7
#  Usage:  powershell -ExecutionPolicy Bypass -File tools\draw_wiring.ps1
# ============================================================
Add-Type -AssemblyName System.Drawing

$out = Join-Path $PSScriptRoot '..\docs\wiring.png'
$W = 1400; $H = 1320; $S = 2

$bmp = New-Object System.Drawing.Bitmap ([int]($W*$S)), ([int]($H*$S))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
$g.ScaleTransform($S, $S)
$g.Clear([System.Drawing.Color]::White)

function Col([string]$h) { [System.Drawing.ColorTranslator]::FromHtml($h) }
function MkFont([single]$sz, [string]$fam, [bool]$bold) {
  $st = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
  New-Object System.Drawing.Font($fam, $sz, $st, [System.Drawing.GraphicsUnit]::Pixel)
}
function RRect([single]$x,[single]$y,[single]$w,[single]$h,[single]$r) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = $r*2
  $p.AddArc($x, $y, $d, $d, 180, 90); $p.AddArc($x+$w-$d, $y, $d, $d, 270, 90)
  $p.AddArc($x+$w-$d, $y+$h-$d, $d, $d, 0, 90); $p.AddArc($x, $y+$h-$d, $d, $d, 90, 90)
  $p.CloseFigure(); return $p
}
function BR([string]$h) { New-Object System.Drawing.SolidBrush ((Col $h)) }
function PN([string]$h,[single]$w) { New-Object System.Drawing.Pen ((Col $h), $w) }

$MONO = 'Consolas'; $SANS = 'Segoe UI'
$fH1   = MkFont 26 $SANS $true
$fH2   = MkFont 13 $SANS $false
$fBar  = MkFont 15 $SANS $true
$fBar2 = MkFont 12 $SANS $false
$fPin  = MkFont 14 $MONO $true
$fPinS = MkFont 11 $MONO $true
$fNet  = MkFont 12 $MONO $false
$fTgt  = MkFont 14 $SANS $false
$fNote = MkFont 11 $SANS $false
$fLeg  = MkFont 12 $SANS $false
$fLegH = MkFont 13 $SANS $true
$fTbL  = MkFont 10 $SANS $false
$fTbV  = MkFont 12 $SANS $true

$bInk   = BR '#111827'
$bGrey  = BR '#6B7280'
$bLight = BR '#9CA3AF'
$bWhite = [System.Drawing.Brushes]::White
$bHead  = BR '#1F2937'

$sfN = New-Object System.Drawing.StringFormat; $sfN.Alignment='Near';   $sfN.LineAlignment='Center'
$sfF = New-Object System.Drawing.StringFormat; $sfF.Alignment='Far';    $sfF.LineAlignment='Center'
$sfC = New-Object System.Drawing.StringFormat; $sfC.Alignment='Center'; $sfC.LineAlignment='Center'
$warn = New-Object System.Collections.ArrayList
function Chk($txt,$font,[single]$maxW,[string]$tag) {
  if ($null -eq $txt -or $txt -eq '') { return }
  $sz = $g.MeasureString($txt, $font)
  if ($sz.Width -gt $maxW) { [void]$warn.Add(("{0}: '{1}' {2:N0}px > {3:N0}px" -f $tag,$txt,$sz.Width,$maxW)) }
}
function Txt([string]$t,$font,$brush,[single]$x,[single]$y,$sf) {
  $g.DrawString($t, $font, $brush, (New-Object System.Drawing.RectangleF($x, ($y-10), 2000, 20)), $sf)
}

# ---------------- sheet frame ----------------
$g.DrawRectangle((PN '#9CA3AF' 1), 14, 14, ($W-28), ($H-28))
$g.DrawRectangle((PN '#374151' 2), 26, 26, ($W-52), ($H-52))
$penTick = PN '#9CA3AF' 1
for ($x = 100; $x -lt ($W-60); $x += 100) {
  $g.DrawLine($penTick, $x, 26, $x, 36); $g.DrawLine($penTick, $x, ($H-36), $x, ($H-26))
}
for ($y = 100; $y -lt ($H-60); $y += 100) {
  $g.DrawLine($penTick, 26, $y, 36, $y); $g.DrawLine($penTick, ($W-36), $y, ($W-26), $y)
}
$zcol = '123456789ABCDEFGHJK'
$zi = 0
for ($x = 100; $x -lt ($W-60); $x += 100) {
  Txt ([string]$zcol[$zi]) $fTbL $bLight ($x-8) 20 $sfC
  Txt ([string]$zcol[$zi]) $fTbL $bLight ($x-8) ($H-20) $sfC
  $zi++
}
$zi = 0
for ($y = 100; $y -lt ($H-60); $y += 100) {
  Txt ([string]$zcol[$zi]) $fTbL $bLight 20 $y $sfC
  Txt ([string]$zcol[$zi]) $fTbL $bLight ($W-20) $y $sfC
  $zi++
}

# ---------------- header ----------------
Txt 'SERVO CONTROLLER - WIRING DIAGRAM' $fH1 $bInk 60 62 $sfN
Txt 'STM32F103C8T6 (Blue Pill)  /  SSD1306 OLED  /  MG90S servo  /  10k pot  /  3 buttons  /  buzzer  /  3 LEDs' $fH2 $bGrey 60 92 $sfN

# ---------------- data ----------------
$blocks = @(
 @{ ref='RV1'; name='10k POTENTIOMETER'; rows=@(
    @{p='PA0'; net='ADC_POT'; tgt='WIPER  (PIN 2)'; c='#2563EB'; n='ADC_IN0'} ) },
 @{ ref='SW1 SW2 SW3'; name='PUSH BUTTONS'; rows=@(
    @{p='PA1'; net='KEY_LEFT';  tgt='SW1   A'; c='#0891B2'; n='INTERNAL PULL-UP'},
    @{p='PA2'; net='KEY_RIGHT'; tgt='SW2   A'; c='#0891B2'; n=''},
    @{p='PA3'; net='KEY_PAGE';  tgt='SW3   A'; c='#0891B2'; n=''} ) },
 @{ ref='DS1'; name='OLED  SSD1306  128x64'; rows=@(
    @{p='PA6'; net='I2C_SDA'; tgt='SDA'; c='#7C3AED'; n='BIT-BANGED I2C'},
    @{p='PA7'; net='I2C_SCL'; tgt='SCL'; c='#7C3AED'; n=''} ) },
 @{ ref='BZ1'; name='BUZZER MODULE'; rows=@(
    @{p='PB0'; net='BUZZER'; tgt='I/O'; c='#0D9488'; n='ACTIVE-LOW'} ) },
 @{ ref='D1 D2 D3'; name='PAGE INDICATOR LEDS'; rows=@(
    @{p='PB1';  net='LED_P1'; tgt='D1   ANODE'; c='#DB2777'; n='1k SERIES RESISTOR'},
    @{p='PB12'; net='LED_P2'; tgt='D2   ANODE'; c='#DB2777'; n=''},
    @{p='PB13'; net='LED_P3'; tgt='D3   ANODE'; c='#DB2777'; n=''} ) },
 @{ ref='M1'; name='SERVO  MG90S'; rows=@(
    @{p='PB6';         net='SERVO_PWM'; tgt='SIGNAL  (ORANGE)'; c='#D97706'; n='TIM4_CH1   50 Hz'},
    @{p='PB7 PB8 PB9'; net='SERVO_PWM'; tgt='ALT SIGNAL';       c='#D97706'; n='SAME OUTPUT'} ) },
 @{ ref='SUPPLY'; name='POWER + GROUND'; rows=@(
    @{p='5V';  net='+5V';  tgt='M1 / DS1 / BZ1   VCC'; c='#DC2626'; n='SERVO MUST BE 5 V'},
    @{p='3V3'; net='+3V3'; tgt='RV1   PIN 1';          c='#EA580C'; n=''},
    @{p='GND'; net='GND';  tgt='ALL GROUNDS';          c='#111827'; n='COMMON GROUND'} ) }
)

# ---------------- geometry ----------------
$mcuX = 70; $mcuW = 260; $mcuTop = 130
$rowH = 36; $blkHdr = 26; $blkPad = 10; $blkGap = 16
$blkX = 600; $blkW = 470
$wireA = $mcuX + $mcuW + 6
$wireB = $blkX
$noteX = $blkX + 12

# 计算总高度
$total = 0
foreach ($b in $blocks) { $total += $blkHdr + ($b.rows.Count * $rowH) + $blkPad + $blkGap }
$mcuBot = $mcuTop + 60 + $total - $blkGap + 12
if ($mcuBot -lt 1100) { $mcuBot = 1100 }

# ---------------- MCU block ----------------
$g.FillPath((BR '#1F2937'), (RRect $mcuX $mcuTop $mcuW ($mcuBot-$mcuTop) 6))
$g.DrawPath((PN '#0F172A' 1.5), (RRect $mcuX $mcuTop $mcuW ($mcuBot-$mcuTop) 6))
$g.FillRectangle((BR '#0F172A'), $mcuX, $mcuTop, $mcuW, 46)
Txt 'U1' $fBar $bWhite ($mcuX+12) ($mcuTop+16) $sfN
Txt 'STM32F103C8T6' $fBar $bWhite ($mcuX+$mcuW-12) ($mcuTop+16) $sfF
Txt 'BLUE PILL  -  ST-LINK  -  72 MHz' $fBar2 (BR '#94A3B8') ($mcuX+12) ($mcuTop+36) $sfN

# ---------------- blocks + wires ----------------
$y = $mcuTop + 60
$firstRowY = $null
foreach ($b in $blocks) {
  $by = $y
  $bh = $blkHdr + ($b.rows.Count * $rowH) + $blkPad
  # block body
  $g.FillPath((BR '#FFFFFF'), (RRect $blkX $by $blkW $bh 5))
  $g.DrawPath((PN '#94A3B8' 1.2), (RRect $blkX $by $blkW $bh 5))
  $g.FillRectangle((BR '#F1F5F9'), $blkX, $by, $blkW, $blkHdr)
  $g.DrawLine((PN '#94A3B8' 1.2), $blkX, ($by+$blkHdr), ($blkX+$blkW), ($by+$blkHdr))
  Txt $b.ref $fBar $bInk ($blkX+12) ($by+13) $sfN
  Txt $b.name $fBar2 $bGrey ($blkX+$blkW-12) ($by+13) $sfF

  $ry = $by + $blkHdr + ($rowH/2) + 4
  foreach ($r in $b.rows) {
    if ($null -eq $firstRowY) { $firstRowY = $ry }
    $pen = PN $r.c 2.4
    $brc = BR $r.c
    # wire
    $g.DrawLine($pen, $wireA, $ry, $wireB, $ry)
    $g.FillEllipse($brc, ($wireA-4.5), ($ry-4.5), 9, 9)
    $g.FillEllipse($brc, ($wireB-4.5), ($ry-4.5), 9, 9)
    # MCU pin label
    $pf = if ($r.p.Length -gt 8) { $fPinS } else { $fPin }
    Chk $r.p $pf ($mcuW-34) ('pin ' + $r.p)
    Txt $r.p $pf $bWhite ($mcuX+$mcuW-14) $ry $sfF
    # net label above the wire
    Chk $r.net $fNet 150 ('net ' + $r.net)
    Txt $r.net $fNet (BR $r.c) ($wireA+8) ($ry-12) $sfN
    # target + note inside block
    Chk $r.tgt $fTgt 200 ('tgt ' + $r.p)
    Txt $r.tgt $fTgt $bInk ($noteX) $ry $sfN
    if ($r.n) {
      Chk $r.n $fNote 190 ('note ' + $r.p)
      Txt $r.n $fNote $bGrey ($blkX+$blkW-12) $ry $sfF
    }
    $ry += $rowH
  }
  $y = $by + $bh + $blkGap
}

# ---------------- legend ----------------
$lx = 40; $ly = 1130; $lw = 800; $lh = 136
$g.FillPath((BR '#F8FAFC'), (RRect $lx $ly $lw $lh 5))
$g.DrawPath((PN '#94A3B8' 1.2), (RRect $lx $ly $lw $lh 5))
Txt 'LEGEND / NOTES' $fLegH $bInk ($lx+14) ($ly+16) $sfN
$leg = @(
 'Wire colours identify signal groups only - they are not the physical wire colours.',
 'Servo cable:  brown / black = GND,   red = VCC (5 V),   orange / white / yellow = SIGNAL.',
 'Buttons:  one leg to the MCU pin, the other leg to GND (internal pull-up, no external resistor).',
 'Potentiometer:  ends to +3V3 and GND, wiper to PA0.',
 'LEDs:  anode through a 1k resistor to the MCU pin, cathode to GND.',
 'WARNING:  the servo supply must share ground with the board. A stalled MG90S can draw up to 1 A'
)
$lyy = $ly + 38
foreach ($t in $leg) { Chk $t $fLeg ($lw-30) 'legend'; Txt $t $fLeg $bInk ($lx+14) $lyy $sfN; $lyy += 16 }

# ---------------- title block ----------------
$tx = 860; $tw = $W - 26 - $tx; $ty = 1130; $th = 136
$g.FillPath((BR '#FFFFFF'), (RRect $tx $ty $tw $th 5))
$g.DrawPath((PN '#374151' 1.5), (RRect $tx $ty $tw $th 5))
$rowsTb = @(
  @{l='PROJECT'; v='STM32-SERVO-OLED'},
  @{l='TITLE';   v='SERVO CONTROLLER - WIRING DIAGRAM'},
  @{l='MCU';     v='STM32F103C8T6  (BLUE PILL)  @ 72 MHz'},
  @{l='SHEET';   v='1 OF 1'},
  @{l='REV';     v='1.0'},
  @{l='DATE';    v='2026-10-15'},
  @{l='DRAWN';   v='NSXSUEK-JPG'}
)
$rh = ($th - 10) / $rowsTb.Count
$tyy = $ty + 5
$lw2 = 74
foreach ($r in $rowsTb) {
  $g.DrawLine((PN '#E5E7EB' 1), ($tx+6), $tyy, ($tx+$tw-6), $tyy)
  Txt $r.l $fTbL $bGrey ($tx+14) ($tyy+$rh/2) $sfN
  Chk $r.v $fTbV ($tw-$lw2-24) ('tb ' + $r.l)
  Txt $r.v $fTbV $bInk ($tx+$lw2) ($tyy+$rh/2) $sfN
  $tyy += $rh
}
$g.DrawLine((PN '#E5E7EB' 1), ($tx+6), $tyy, ($tx+$tw-6), $tyy)
$g.DrawLine((PN '#E5E7EB' 1), ($tx+$lw2-6), ($ty+5), ($tx+$lw2-6), $tyy)

$g.Dispose()
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Output ('saved: ' + (Resolve-Path $out).Path)
Write-Output ('size : ' + [math]::Round((Get-Item $out).Length/1KB) + ' KB   (' + $W + 'x' + $H + ' logical, ' + ($W*$S) + 'x' + ($H*$S) + ' px)')
if ($y -gt 1120) { Write-Output ('GEOMETRY WARNING: content ends at y=' + $y + ', overlaps legend at 1130') } else { Write-Output ('geometry check: content ends at y=' + ($y - $blkGap) + '  (legend/title start at 1130)  OK') }
if ($warn.Count -eq 0) { Write-Output 'text-fit check: OK (nothing overflows)' }
else { Write-Output 'text-fit check:'; $warn | ForEach-Object { '  ' + $_ } }
