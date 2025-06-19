# Google Sheets Integration for Gen3 ESP32 Trip Logger
This module enables secure logging of trip data from an ESP32 to Google Sheets using a Service Account. It also includes a Google Apps Script that processes and visualizes trip data.

The ESP32 connects to your phone hotspot WiFi, authenticates with Google using a Service Account's private key (reading JSON file from SD card), obtains an OAuth 2.0 access token, and then uses the Google Sheets API to append a new row of data to a specific sheet. This method is secure and does not require exposing any private keys in a publicly accessible script.

You can follow this [tutorial](https://randomnerdtutorials.com/esp32-datalogging-google-sheets) to create Google Service Account.<br/>
Instead of creating a blank worksheet, you can make a copy of this pre-configured [template.](https://docs.google.com/spreadsheets/d/1rQq4N23tIi17RG1ABo95-n2uzZKbinA9Vr6XkvBrGoo/edit?gid=172316373#gid=172316373), which includes the necessary [Google Apps Script](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/Google-spreadsheet/Gen3logSorter.gs). After copying, edit the script to update the spreadsheet ID, then run the macro **InsertTriggers** and grant the required permissions.
You should also share the spreadsheet with Service account you created so he can append data at the end of the data sheet. The macro **Sorter** is started by Trigger every 5 minutes and if new data is arrived it will populate rest of the data and sort it so newest data will always be on the top of the sheet.


**Trip Data Sheet Example**
**![SampleData.png](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/Google-spreadsheet/SampleData.png?raw=true)**

**Generated Trip Map from selected trip in cell A1**
**![SampleMap.png](https://github.com/DejanVasic/Gen3-Trip-Logger/blob/master/Google-spreadsheet/SampleMap.png?raw=true)**


