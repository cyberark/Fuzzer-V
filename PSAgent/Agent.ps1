$INPUT_PATH = "\\tsclient\C\Hyper-V\input\input.bin"
$AGENT_INPUT_PATH = "C:\Hyper-V\PSAgent\agent_input"
$CHANNEL_DEFINITION_PATH = "C:\Hyper-V\PSAgent\channel_definition"
$AGENT_PATH = "C:\Hyper-V\Agent\Agent.exe"
# $DEVICE_NAME = "\device\HyperVAgent"
$EVENT_NAME = "Global\new_input_event"

# function CTL_CODE 
# {
#     param ([int]$DeviceType, [int]$Function, [int]$Method, [int]$Access)
    
#     return (($DeviceType) -shl 16) -bor (($Access) -shl 14) -bor (($Function) -shl 2) -bor ($Method)
# }

# $INIT_IOCTL = CTL_CODE 0x22 0x01 0x00 0x00
# $GET_CHANNEL_IOCTL = CTL_CODE 0x22 0x02 0x00 0x00
# $SEND_PACKET_IOCTL = CTL_CODE 0x22 0x03 0x00 0x00



# $DEVICE = Get-NtFile $DEVICE_NAME

# # send the init IOCTL (input does not matter)
# $DEVICE.DeviceIoControl($INIT_IOCTL, 5, 5)

# # read the channel definition from a file
# $input_bytes = [System.IO.File]::ReadAllBytes($CHANNEL_DEFINITION_PATH)
# $size = $input_bytes.Length
# # send the channel definition IOCTL
# $DEVICE.DeviceIoControl($GET_CHANNEL_IOCTL, $input_bytes, $size)


# start the c agent
Start-Process powershell -ArgumentList "-noexit -command [console]::windowwidth=50; [console]::windowheight=20; Start-Process -FilePath $AGENT_PATH -NoNewWindow -ArgumentList $EVENT_NAME, $AGENT_INPUT_PATH, $CHANNEL_DEFINITION_PATH"

# wait until the agent creates the event
$new_input_event = $null
while ($null -eq $new_input_event)
{
    # let the c agent init (overkill sleep but one time)
    Start-Sleep -Seconds 1

    # open the sync event
    $new_input_event = [System.Threading.EventWaitHandle]::OpenExisting($EVENT_NAME)
}

while ($true)
{
    # wait until we have a new input
    if (Test-Path $INPUT_PATH)
    {
        echo "new input" > "log.txt"
        # # read new input
        # $input_bytes = [System.IO.File]::ReadAllBytes($INPUT_PATH)
        # $size = $input_bytes.Length

        # # delete the file 
        # Move-Item -Path $INPUT_PATH -Destination $AGENT_INPUT_PATH

        # # send input IOCTL
        # $DEVICE.DeviceIoControl($SEND_PACKET_IOCTL, $input_bytes, $size)
    
        # rename input to what the c agent expects
        Move-Item -Path $INPUT_PATH -Destination $AGENT_INPUT_PATH

        echo "moved input" > "log.txt"
    
        # let the c agent know we have a new input
        $new_input_event.set()

        echo "event set" > "log.txt"

        # wait until the input is deleted
        while (Test-Path $AGENT_INPUT_PATH)
        {
            Start-Sleep -Milliseconds 1
        }

        echo "input deleted" > "log.txt"
    }
    else
    {
        Start-Sleep -Milliseconds 1
    }
}

