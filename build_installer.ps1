param (
    [string]$Version = "v1.0"
)

$ErrorActionPreference = "Stop"

# 환경 변수(PATH)에 빌드 도구들이 존재하는지 확인합니다.
if (!(Get-Command cmake -ErrorAction SilentlyContinue) -or !(Get-Command windeployqt -ErrorAction SilentlyContinue)) {
    Write-Host "[오류] cmake 또는 windeployqt 명령어를 찾을 수 없습니다." -ForegroundColor Red
    Write-Host "이 스크립트는 Qt 컴파일러 환경 변수가 설정된 상태에서 실행되어야 합니다." -ForegroundColor Yellow
    Write-Host "Windows 시작 메뉴에서 'Qt x.x.x (MinGW) Command Prompt'를 검색하여 실행한 뒤, 그곳에서 이 스크립트를 다시 실행해 주세요." -ForegroundColor Yellow
    exit 1
}

Write-Host "1. 빌드 디렉토리 초기화 및 릴리즈 빌드 시작..." -ForegroundColor Cyan
if (Test-Path "build_release") {
    Remove-Item -Recurse -Force "build_release"
}
mkdir "build_release" | Out-Null
cd "build_release"

# CMake 릴리즈 모드로 구성 및 빌드
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

cd ..

Write-Host "2. 배포(Release) 폴더 생성 및 실행 파일 복사..." -ForegroundColor Cyan
$ReleaseDir = "LightCell_$Version"
if (Test-Path $ReleaseDir) {
    Remove-Item -Recurse -Force $ReleaseDir
}
mkdir $ReleaseDir | Out-Null

Copy-Item "build_release\LightCell.exe" -Destination "$ReleaseDir\"

Write-Host "3. Qt 의존성(DLL) 패키징 진행 중 (windeployqt)..." -ForegroundColor Cyan
# windeployqt를 사용하여 필요한 dll을 복사
windeployqt --compiler-runtime "$ReleaseDir\LightCell.exe"

# 추가 리소스 복사 (사용자 설명서 등)
Copy-Item "README.md" -Destination "$ReleaseDir\" -ErrorAction SilentlyContinue

Write-Host "4. 배포용 압축 파일(ZIP) 생성..." -ForegroundColor Cyan
$ZipFile = "LightCell_$Version.zip"
if (Test-Path $ZipFile) {
    Remove-Item -Force $ZipFile
}
Compress-Archive -Path "$ReleaseDir\*" -DestinationPath $ZipFile

Write-Host "완료! 배포 파일이 생성되었습니다: $ZipFile" -ForegroundColor Green
