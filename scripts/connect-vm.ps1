while ($true) {
    & "C:\Program Files\PuTTY\putty.exe" -serial "\\.\pipe\NovaLogPipe" -sercfg 115200,8,n,1,N
    Start-Sleep -Seconds 2
}