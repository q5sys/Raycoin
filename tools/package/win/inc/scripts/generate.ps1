$path = $(Resolve-Path $($PSScriptRoot + "/.."))
& "$path/scripts/settings.ps1"
if (Test-Path "$env:APPDATA/Raycoin/settings.ps1") { & "$env:APPDATA/Raycoin/settings.ps1" }

$sleep = if ($($args.Count) -ge 1) { $(Get-Item env:"SLEEP_$($args[0].ToUpper())").Value } else { $env:SLEEP }

if (!(Get-Process raycoind -ErrorAction Ignore))
{
	Start-Process "$path/scripts/Raycoin.lnk"
	Start-Sleep -s 30
}
& "$path/scripts/stop-generate.ps1"
Start-Sleep -s 1

(1..$env:GPU_COUNT) | %{
	$dir = if ($_ -eq 1) { "" } else { $_ }
	$num = $_ - 1
	$args = "-datadir=`"$env:APPDATA/Raycoin$dir`" -rpcport=$(8381 + $num*2)"
	Start-Process -WindowStyle Hidden "$path/raycoin-cli" -ArgumentList $args, generate, $env:MINING_ADDRESS, $sleep
}