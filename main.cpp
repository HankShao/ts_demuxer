#include <iostream>
#include <fstream>
#include "ts_demuxer.h"

using namespace std;

int main(int argc, char *argv[])
{
	ofstream file("./1.h264", ios::out);
	cout << "input:" << argv[1] << endl;
	Handle hnd;
	hnd = TS_Open(argv[1]);
	if (NULL == hnd)
		cout << argv[1] << "open failed!" << endl;

	ts_file_info_s fileinfo;
	TS_GetFileInfo(hnd, &fileinfo);
	TS_SetTimePosition(hnd);
	char *frm;
	int len;
	int cnt = 0;
	int frame = 0;
	while(cnt < 50)
	{
		if (frame++%5 == 0)
			TS_SeekPos(hnd, fileinfo.start_time + 12 * 1000 * cnt, 0);
		if (0 != TS_ReadFrame(hnd, (void **)(&frm), &len))
			break;
		file.write(frm, len);
		TS_ReleaseFrame(hnd, frm);
		cnt++;
	}
	file.close();
	
	return 0;
}
