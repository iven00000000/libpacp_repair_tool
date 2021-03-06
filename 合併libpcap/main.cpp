#include <fstream>
#include <iostream>
#include <string>
#include <ctime>
#include <cstdio>

#define HeaderBytes 24
#define MaxPkgBytes 65544  //65536+8
#define KeepDays 7
#define KeepSeconds (KeepDays*86400)
#define StartTimeOffset (-1*86400)  // -1 day
#define MaxBufSize (1024*1024*1024) // 1 GB

using namespace std;

struct charArr{
	charArr(){}
	charArr(int initSize){ data = new char[initSize]; }
	int size = 0;
	char *data;
};

struct pkg : charArr{
	pkg(){ data = new char[MaxPkgBytes]; }
};

int catoi(const char* ca){
	char tmp[4];
	int* iptr;
	for (int i = 0; i < 4; i++){
		tmp[i] = ca[3 - i];
	}
	iptr = reinterpret_cast<int*>(tmp);
	return *iptr;
}

#ifdef _MSC_VER
#include <windows.h>
#include <iomanip>
wstring str2wstr(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	wstring wstr(buf);
	return wstr;
}
#else
#define memcpy_s(dest,destSize,src,count) memcpy(dest, src, count)
#endif // _MSC_VER


int main(int argc, char** argv){
	string inFileName, outFileName;
	fstream fs_in, fs_out;
	char buf_char;
	int buf_int, headercount = 0, curPkgIdx= 0, lastPkgIdx = 1, tmp;
	bool isBroken = false, isValid, isHeader;
	clock_t mytime;
	unsigned int StartTime = 0, PkgTime;
	pkg buf_pkg[2];
	charArr outBuf(MaxBufSize);

	if (argc != 2){
		return 1;
	}


	inFileName = argv[1];
	fs_in.open(inFileName, ios::binary | ios::in);
	if (!fs_in){
		cout << "Can't open the file: " << inFileName << endl;
		return 1;
	}
	fs_in.seekg(0, fs_in.end);
	cout << "input file size: " << fs_in.tellg() << endl;
	fs_in.seekg(0, fs_in.beg);

	outFileName = inFileName;
	outFileName.insert(outFileName.rfind('.'), "_integrated");
	fs_out.open(outFileName, ios::binary | ios::out);
	if (!fs_out){
		cout << "Can't open the file: " << outFileName << endl;
		return 1;
	}


	int invalidPConuter = 0;
	long long outBufMaxPos = 0;

	
	buf_pkg[0].size = 0;
	buf_pkg[1].size = 0;

	mytime = clock();
	fs_in.read(buf_pkg[curPkgIdx].data, HeaderBytes);
	memcpy_s(outBuf.data + outBuf.size, MaxBufSize - outBuf.size, buf_pkg[curPkgIdx].data, HeaderBytes);
	outBuf.size += HeaderBytes;
	if (fs_in){
		fs_in.read(buf_pkg[curPkgIdx].data, 4);
		StartTime = catoi(buf_pkg[curPkgIdx].data);
		StartTime += StartTimeOffset;
		fs_in.seekg(-4, ios_base::cur);
	}
	cout << "start" << endl;
	while (fs_in.get(buf_char)){
		fs_in.seekg(-1, ios_base::cur);
		if (buf_char == -95 ){    //0xa1
			fs_in.read(reinterpret_cast<char*>(&buf_int), sizeof(int));
 			if (buf_int == 0xd4c3b2a1){  //a1b2 c3d4
				fs_in.seekg(HeaderBytes-4, ios_base::cur);
				headercount++;
				isHeader = true;
			}
			else
			{
				fs_in.seekg(-4, ios_base::cur);
				isHeader = false;
			}
		}
		else isHeader = false;
		if(!isHeader){
			fs_in.read(buf_pkg[curPkgIdx].data, 16);
			PkgTime = catoi(buf_pkg[curPkgIdx].data);

			/*Set isValid*/
			if (PkgTime - StartTime < KeepSeconds) isValid = true;
			else isValid = false;

			if (isValid){  //last packetage is valid
				/*write last packetage data*/
				if (buf_pkg[lastPkgIdx].size)
				{
					if (buf_pkg[lastPkgIdx].size + outBuf.size > MaxBufSize)
					{
						cout << "write" << endl;
						fs_out.write(outBuf.data, outBuf.size);
						outBuf.size = 0;  //reset outBuf
					}
					memcpy_s(outBuf.data + outBuf.size, MaxBufSize - outBuf.size, buf_pkg[lastPkgIdx].data, buf_pkg[lastPkgIdx].size);
					outBuf.size += buf_pkg[lastPkgIdx].size;
					buf_pkg[lastPkgIdx].size = 0;
				}
				/*write last packetage data*/

				/*store size of packet*/
				buf_pkg[curPkgIdx].size = catoi(buf_pkg[curPkgIdx].data + 8);
				/*store size of packet*/
				if (buf_pkg[curPkgIdx].size > MaxPkgBytes || buf_pkg[curPkgIdx].size <= 0) isValid = false; // current paceket is not valid
				else
				{
					/*read packet data*/
					fs_in.read(buf_pkg[curPkgIdx].data + 16, buf_pkg[curPkgIdx].size);
					buf_pkg[curPkgIdx].size += 16;
					/*read packet data*/

					/*swap idx of buffer*/
					tmp = curPkgIdx;
					curPkgIdx = lastPkgIdx;
					lastPkgIdx = tmp;
					/*swap idx of buffer*/
				}
			}
			if (!isValid)
			{
				++invalidPConuter;
				isBroken = true;
				fs_in.seekg(-buf_pkg[lastPkgIdx].size - 15, ios_base::cur);

				/*search correct packetage byte by byte*/

				/*Let PkgTime be invalid.
				If packet is invalid because of its size, original PkgTime was valid*/
				PkgTime = StartTime + KeepSeconds; 

				while (PkgTime - StartTime >= KeepSeconds && fs_in.read(buf_pkg[curPkgIdx].data, 4)){
					PkgTime = catoi(buf_pkg[curPkgIdx].data);
					fs_in.seekg(-3, ios_base::cur);
				}
				fs_in.seekg(-1, ios_base::cur);
				/*search correct packetage byte by byte*/

				buf_pkg[lastPkgIdx].size = 0; //reset the size of the invalid packetage
			}
		}
	}
	fs_in.close();

	mytime = clock() - mytime;
	cout << "Repair pacp: " << mytime << " miniseconds." << endl;
	cout << "Number of deleted headers: " << headercount << endl;
	cout << "Number of broken packet: " << invalidPConuter << endl;


	mytime = clock();

	if (headercount || isBroken){
		fs_out.write(outBuf.data, outBuf.size);
		fs_out.close();
#ifdef _MSC_VER
		wstring originFileName, newFileName;
		originFileName = str2wstr(inFileName);
		newFileName = str2wstr(inFileName.insert(inFileName.rfind("."), "_origin"));

		int flag = MoveFileExW(originFileName.c_str(), newFileName.c_str(), 0);
		if (!flag)
		{
			cout << "fail to rename origin file" << endl;
			cout << showbase // show the 0x prefix
				<< internal // fill between the prefix and the number
				<< setfill('0'); // fill with 0s
			cout << "Error code: " << hex << setw(4) << GetLastError() << dec << endl;
		}
		else
		{
			newFileName = originFileName;
			originFileName = str2wstr(outFileName);
			flag = MoveFileExW(originFileName.c_str(), newFileName.c_str(), 0);
			if (!flag)
			{
				cout << "fail to rename output file" << endl;
				cout << showbase // show the 0x prefix
					<< internal // fill between the prefix and the number
					<< setfill('0'); // fill with 0s
				cout << "Error code: " << hex << setw(4) << GetLastError() << dec << endl;
			}
		}

#endif //_MSC_VER 		

	}
	else
	{
		wstring tmpwstr = str2wstr(outFileName);
		fs_out.close();
		if (!DeleteFileW(tmpwstr.c_str()))
		{
			cout << "Cannot deleted tmp file (integrated)" << endl;
		}
		cout << "The file is completed. Do nothing." << endl;
	}

	mytime = clock() - mytime;
	cout << "Rename file: " << mytime << " miniseconds." << endl;
	system("pause"); 
	return 0;

}