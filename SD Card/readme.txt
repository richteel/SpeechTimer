The config.txt is plain text file. Anyone who accesses this file will have the information needed to connect to your WiFi network, so use with caution.

Below is sample content for the config.txt file.

{
	"hostname": "SpeechTimer",
	"networks": [
		{
			"ssid": "SSID_1",
			"pass": "SSID_1 Password"
		},
		{
			"ssid": "SSID_2",
			"pass": "SSID_2 Password"
		},
		{
			"ssid": "SSID_3",
			"pass": "SSID_3 Password"
		}
	],
	"timers": [
		{
			"min": 5,
			"max": 7,
			"manual": false
		},
		{
			"min": 1,
			"max": 2,
			"manual": false
		},
		{
			"min": 2,
			"max": 3,
			"manual": false
		},
		{
			"min": 0,
			"max": 0,
			"manual": true
		}
	]
}