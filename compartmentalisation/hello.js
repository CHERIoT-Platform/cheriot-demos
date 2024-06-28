// Import everything from the environment
import * as host from "./cheri.js"

var run_once = false
var tick = 0;
var direction = false;

function run()
{
	if (!run_once)
	{
		host.print("Hello world!")
		run_once = true
		for (var i = 0; i < 8; i++)
		{
			host.led_set(i, false)
		}
	}

	var led = tick % 8
	if (led === 0)
	{
		direction = !direction
	}
	host.print("Setting LED ", led, " to ", direction)
	host.led_set(led, direction)
	tick++
}


vmExport(1234, run);
