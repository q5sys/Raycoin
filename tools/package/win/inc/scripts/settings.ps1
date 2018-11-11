#The wallet address that will receive your mining rewards.
#Set this to a receiving address from your electrum wallet.
#Default "0" will use addresses generated from raycoin's command-line wallet.
#ex. "3HwpSFbMb9P1eKFiE2SnSbbYpXS5mj22e7"

$env:MINING_ADDRESS = "0"

#Sleep duration between hash attempts in microseconds for the various mining speeds.
#Longer sleeps are less demanding on the GPU and can help with desktop usability or thermal issues.

$env:SLEEP = 1000
$env:SLEEP_BACKGROUND = 100000
$env:SLEEP_FULLSPEED = 0

#Custom arguments to pass to raycoin.

$env:DAEMON_ARGS = ""

#Number of GPUs to use for multi-GPU setups.
#A raycoin instance will be spawned for each GPU with data at: <USER>/AppData/Roaming/Raycoin<#>/

$env:GPU_COUNT = 1