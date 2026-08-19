# Test script for BuildConfigs.json parsing validation
# Tests requirements 6.4, 6.5, 6.6, 6.7, 6.8, 6.9, 6.10

Write-Host "Testing BuildConfigs.json parsing implementation..." -ForegroundColor Cyan
Write-Host ""

$testsPassed = 0
$testsFailed = 0

# Test 1: Default config (LIGHTING_rev5) should work
Write-Host "Test 1: Default config (LIGHTING_rev5)" -ForegroundColor Yellow
$output = cmake -S . -B build/test1 2>&1 | Out-String
if ($output -match "Using BuildConfig: LIGHTING_rev5" -and 
    $output -match "target_location: targets/efs-can-lighting" -and
    $output -match "board: stm32l4" -and
    $output -match "REV5=1") {
    Write-Host "  PASS: Default config parsed correctly" -ForegroundColor Green
    $testsPassed++
} else {
    Write-Host "  FAIL: Default config not parsed correctly" -ForegroundColor Red
    $testsFailed++
}
Write-Host ""

# Test 2: LIGHTING_rev4 should work
Write-Host "Test 2: LIGHTING_rev4 config" -ForegroundColor Yellow
$output = cmake -S . -B build/test2 -DPINECAN_CONFIG_KEY=LIGHTING_rev4 2>&1 | Out-String
if ($output -match "Using BuildConfig: LIGHTING_rev4" -and
    $output -match "REV4=1" -and
    $output -notmatch "REV5=1") {
    Write-Host "  PASS: LIGHTING_rev4 parsed correctly" -ForegroundColor Green
    $testsPassed++
} else {
    Write-Host "  FAIL: LIGHTING_rev4 not parsed correctly" -ForegroundColor Red
    $testsFailed++
}
Write-Host ""

# Test 3: Invalid config key should fail
Write-Host "Test 3: Invalid config key (INVALID_KEY)" -ForegroundColor Yellow
$output = cmake -S . -B build/test3 -DPINECAN_CONFIG_KEY=INVALID_KEY 2>&1 | Out-String
if ($output -match "Config key 'INVALID_KEY' not found") {
    Write-Host "  PASS: Invalid key rejected" -ForegroundColor Green
    $testsPassed++
} else {
    Write-Host "  FAIL: Invalid key not rejected" -ForegroundColor Red
    $testsFailed++
}
Write-Host ""

# Test 4: Wrong target_location should fail
Write-Host "Test 4: Wrong target_location (SSD_rev1)" -ForegroundColor Yellow
$output = cmake -S . -B build/test4 -DPINECAN_CONFIG_KEY=SSD_rev1 2>&1 | Out-String
if ($output -match "has target_location" -and $output -match "expected 'targets/efs-can-lighting'") {
    Write-Host "  PASS: Wrong target_location rejected" -ForegroundColor Green
    $testsPassed++
} else {
    Write-Host "  FAIL: Wrong target_location not rejected" -ForegroundColor Red
    $testsFailed++
}
Write-Host ""

# Test 5: Verify key symbols are reported
Write-Host "Test 5: Key symbols reported" -ForegroundColor Yellow
$output = cmake -S . -B build/test5 -DPINECAN_CONFIG_KEY=LIGHTING_rev5 2>&1 | Out-String
$keySymbols = @("NODE_NAME", "NODE_ID", "REV5", "CANARD_MEM_POOL_SIZE")
$allFound = $true
foreach ($sym in $keySymbols) {
    if ($output -notmatch [regex]::Escape($sym)) {
        $allFound = $false
        Write-Host "  Missing symbol: $sym" -ForegroundColor Red
    }
}
if ($allFound) {
    Write-Host "  PASS: Key symbols reported" -ForegroundColor Green
    $testsPassed++
} else {
    Write-Host "  FAIL: Some symbols missing" -ForegroundColor Red
    $testsFailed++
}
Write-Host ""

# Clean up test directories
Write-Host "Cleaning up test directories..." -ForegroundColor Gray
Remove-Item -Recurse -Force build/test* -ErrorAction SilentlyContinue

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Test Summary:" -ForegroundColor Cyan
Write-Host "  Passed: $testsPassed" -ForegroundColor Green
Write-Host "  Failed: $testsFailed" -ForegroundColor Red
Write-Host "========================================" -ForegroundColor Cyan

if ($testsFailed -eq 0) {
    Write-Host ""
    Write-Host "All tests passed! Task 4.2 implementation verified." -ForegroundColor Green
    exit 0
} else {
    Write-Host ""
    Write-Host "Some tests failed. Please review the implementation." -ForegroundColor Red
    exit 1
}
