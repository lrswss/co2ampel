//
// Copyright (c) 2020 Lars Wessels
///
// This file a part of the "CO2-Ampel" source code.
// https://github.com/lrswss/co2ampel
//
// Published under Apache License 2.0
//
function Decoder(bytes, port) {
	// Decode an uplink message from a buffer
	// (array) of bytes to an object of fields.
	var decoded = {};
	var i;

	if (bytes.length == bytes[0]) {
		for (i=1; i < bytes.length; i++) {
			switch (bytes[i]) {
				case 0x01:
					switch(bytes[i+1]) {
						case 0:
							decoded.airquality = "nodata";
							break;
						case 1:
							decoded.airquality = "warmup";
							break;
						case 2:
							decoded.airquality = "good";
							break;
						case 3:
							decoded.airquality = "medium";
							break;	
						case 4:
							decoded.airquality = "critical";
							break;	
						case 5:
							decoded.airquality = "alarm";
							break;	
						case 6:
							decoded.airquality = "calibrate";
							break;
						case 7:
							decoded.airquality = "failure";
							break;
						case 8:
							decoded.airquality = "noop";
							break;
					}
					break;
				case 0x10:
					decoded.temperature = ((bytes[i+1] & 0x80 ? 0xffff << 16 : 0) + (bytes[i+1] << 8) + bytes[i+2])/10;
					i = i+2;
					break;
				case 0x11:
					decoded.humidity = bytes[i+1];
					i = i+1;
					break;	
				case 0x12:
					decoded.pressure = bytes[i+1] + 870;
					i = i+1;
					break;
				case 0x13:
					decoded.co2 = (bytes[i+1] << 8) + bytes[i+2];
					i= i+2;
					break;
				case 0x20:
					decoded.vbat = (bytes[i+1]+256)/100;
					i= i+1;
					break;
			}
		}
	}
	return decoded;
}
