waybar-nvidia-monitor(5)

# NAME

waybar - nvidia monitor

# DESCRIPTION

The *nvidia monitor* module gathers data from Nvidia's GPUs via nvidia-smi program.

# CONFIGURATION

For proper use of this module, *nvidia-smi* must be present in the system.
Additionally, the module requires an exchange file to be set up. If a path is not specified, the default is on '/$TEMP/waybar/' or if '$TEMP' is not defined then '/tmp/waybar'.

*data-exchange-path*: ++
  typeof: string ++
  default: '/$TEMP/waybar' or '/tmp/waybar' ++
  The path to the exchange file between 'nvidia-smi' and the module.

*format*: ++
	typeof: string  ++
	default: {gpuTemp}% ++
	The format, how information should be displayed. On {} data gets inserted.

*tooltip*: ++
	typeof: bool ++
	default: true ++
	Option to disable tooltip on hover.

*tooltip-format*: ++
	typeof: string  ++
	default: {gpuTemp}°C ++
	The format for the tooltip

# FORMAT REPLACEMENTS

*{name}*: Current graphics card name.
*{busId}*: The bus id.
*{driverVersion}*: Current driver version, only major and minor numbers.
*{powerState}*: The actual performance state, states range from P0 (max performance) to P12 (minimum performance).
*{pcieLinkMax}*: Maximum PCIE link available in the GPU or in the system.
*{pcieLinkCurr}*: Current PCIE link, could be reduce if GPU is not performing.
*{gpuTemp}*: Current GPU temperature. The format is Celsius.
*{gpuUtilization}*: Current GPU utilization.
*{memUtilization}*: Current memory utilization.
*{memTotal}*: Total amount of the available memory. The format is fixed to MiB.
*{memFree}*: Total amount of the free memory. The format is fixed to MiB.
*{memUsed}*: Total amount of the used memory. The format is fixed to MiB.
*{powerDraw}*: Current power draw presented with Watts.
*{clocksSm}*: Current frequency of streaming multiprocessor clock.
*{clocksMem}*: Current frequency of memory clock.
*{clocksGr}*: Current frequency of graphics (shader) clock.

# EXAMPLES

Example configuration:

```
  "modules-right": [
    "nvidia_monitor",
  ],
  "nvidia_monitor": {
    "format": "{gpuTemp}°C",
    "tooltip": true,
    "tooltip-format": "{name}\n{driverVersion}\n{memTotal}\n{memFree}\n{memUsed}\n{powerDraw}"
  }
```
