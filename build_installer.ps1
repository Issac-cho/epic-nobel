param (
    [string]$Version = "v1.0.1"
)

$ErrorActionPreference = "Stop"

# ?˜ê²½ ë³€??PATH)??ë¹Œë“œ ?„êµ¬?¤ì´ ì¡´ì¬?˜ëŠ”ì§€ ?•ì¸?©ë‹ˆ??
if (!(Get-Command cmake -ErrorAction SilentlyContinue) -or !(Get-Command windeployqt -ErrorAction SilentlyContinue)) {
    Write-Host "[?¤ë¥˜] cmake ?ëŠ” windeployqt ëª…ë ¹?´ë? ì°¾ì„ ???†ìŠµ?ˆë‹¤." -ForegroundColor Red
    Write-Host "???¤í¬ë¦½íŠ¸??Qt ì»´íŒŒ?¼ëŸ¬ ?˜ê²½ ë³€?˜ê? ?¤ì •???íƒœ?ì„œ ?¤í–‰?˜ì–´???©ë‹ˆ??" -ForegroundColor Yellow
    Write-Host "Windows ?œì‘ ë©”ë‰´?ì„œ 'Qt x.x.x (MinGW) Command Prompt'ë¥?ê²€?‰í•˜???¤í–‰???? ê·¸ê³³?ì„œ ???¤í¬ë¦½íŠ¸ë¥??¤ì‹œ ?¤í–‰??ì£¼ì„¸??" -ForegroundColor Yellow
    exit 1
}

Write-Host "1. ë¹Œë“œ ?”ë ‰? ë¦¬ ì´ˆê¸°??ë°?ë¦´ë¦¬ì¦?ë¹Œë“œ ?œì‘..." -ForegroundColor Cyan
if (Test-Path "build_release") {
    Remove-Item -Recurse -Force "build_release"
}
mkdir "build_release" | Out-Null
cd "build_release"

# CMake ë¦´ë¦¬ì¦?ëª¨ë“œë¡?êµ¬ì„± ë°?ë¹Œë“œ
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

cd ..

Write-Host "2. ë°°í¬(Release) ?´ë” ?ì„± ë°??¤í–‰ ?Œì¼ ë³µì‚¬..." -ForegroundColor Cyan
$ReleaseDir = "LightCell_$Version"
if (Test-Path $ReleaseDir) {
    Remove-Item -Recurse -Force $ReleaseDir
}
mkdir $ReleaseDir | Out-Null

Copy-Item "build_release\LightCell.exe" -Destination "$ReleaseDir\"

Write-Host "3. Qt ?˜ì¡´??DLL) ?¨í‚¤ì§?ì§„í–‰ ì¤?(windeployqt)..." -ForegroundColor Cyan
# windeployqtë¥??¬ìš©?˜ì—¬ ?„ìš”??dll??ë³µì‚¬
windeployqt --compiler-runtime "$ReleaseDir\LightCell.exe"

# ì¶”ê? ë¦¬ì†Œ??ë³µì‚¬ (?¬ìš©???¤ëª…????
Copy-Item "README.md" -Destination "$ReleaseDir\" -ErrorAction SilentlyContinue

Write-Host "4. ë°°í¬???•ì¶• ?Œì¼(ZIP) ?ì„±..." -ForegroundColor Cyan
$ZipFile = "LightCell_$Version.zip"
if (Test-Path $ZipFile) {
    Remove-Item -Force $ZipFile
}
Compress-Archive -Path "$ReleaseDir\*" -DestinationPath $ZipFile

Write-Host "?„ë£Œ! ë°°í¬ ?Œì¼???ì„±?˜ì—ˆ?µë‹ˆ?? $ZipFile" -ForegroundColor Green
