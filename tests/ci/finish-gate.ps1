$ErrorActionPreference = 'Stop'
if ($env:OFFLINE_JOB_RESULT -ne 'success' -or
    $env:OFFLINE_VERIFIED -ne 'ci-offline' -or
    [string]::IsNullOrWhiteSpace($env:OFFLINE_TESTED_SHA) -or
    $env:OFFLINE_TESTED_SHA -ne $env:OFFLINE_EXPECTED_SHA) {
    throw "ci-check failed: job=$($env:OFFLINE_JOB_RESULT), evidence=$($env:OFFLINE_VERIFIED), tested=$($env:OFFLINE_TESTED_SHA)"
}
Write-Output 'ci-check PASS (main including VISA, OCR and WFM verified for the same source).'
