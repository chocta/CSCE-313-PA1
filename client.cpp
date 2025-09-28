/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Bhavana Venkatesh
	UIN: 734001598
	Date: 9/23/25
*/
#include "common.h"
#include "FIFORequestChannel.h"

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = 1;
	double t = 0.0;
	int e = 1;
	int buffercap = MAX_MESSAGE;
	bool use_new_channel = false;

	
	string filename = "";

	bool t_given = false;
	bool e_given = false;
	bool f_given = false;
	

	while ((opt = getopt(argc, argv, "p:t:e:f:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				t_given = true;
				break;
			case 'e':
				e = atoi (optarg);
				e_given=true;
				break;
			case 'f':
				filename = optarg;
				f_given=true;
				break;
			case 'c': 
				use_new_channel = true; 
				break;

		}
	}

	//start the server as a child process 
	pid_t pid = fork();
	if (pid == 0) {
		string mstr = to_string(buffercap);
		char* args[] = {(char*)"server", (char*)"-m", (char*)mstr.c_str(), NULL};
		execvp("./server", args);
		perror("execvp failed");
		_exit(1);
	}


    FIFORequestChannel control("control", FIFORequestChannel::CLIENT_SIDE);
	char buf[MAX_MESSAGE];

	FIFORequestChannel* work_chan = &control;
    if (use_new_channel) {
        MESSAGE_TYPE m = NEWCHANNEL_MSG;
        control.cwrite(&m, sizeof(MESSAGE_TYPE));

        char newname[30];
        control.cread(newname, sizeof(newname));
        work_chan = new FIFORequestChannel(newname, FIFORequestChannel::CLIENT_SIDE);
    }
	if (!f_given && !t_given && !e_given && p > 0) {
		mkdir("received", 0777);
		string outpath = "received/x1.csv";
		ofstream out(outpath);
		if (!out) {
			perror("cannot open received/x1.csv");
			return 1;
		}
		for (int i = 0; i < 1000; i++) {
			double time = i * 0.004;
			double v1, v2;

			datamsg m1(p, time, 1);
			work_chan->cwrite(&m1, sizeof(datamsg));
			work_chan->cread(&v1, sizeof(double));

			datamsg m2(p, time, 2);
			work_chan->cwrite(&m2, sizeof(datamsg));
			work_chan->cread(&v2, sizeof(double));

			out << time << "," << v1 << "," << v2 << "\n";
		}
		out.close();
		cout << "Collected 1000 data points into " << outpath << endl;
	}



	else if (filename.empty()) {
        // Single ECG data point
        datamsg x(p, t, e);
        memcpy(buf, &x, sizeof(datamsg));
        work_chan->cwrite(buf, sizeof(datamsg));
        double reply;
        work_chan->cread(&reply, sizeof(double));
        cout << "For person " << p << ", at time " << t
             << ", the value of ecg " << e << " is " << reply << endl;
    }

	else {
        // File download 
        mkdir("received", 0777);

        // request file size
        filemsg fm(0, 0);
        int len = sizeof(filemsg) + filename.size() + 1;
        char* req = new char[len];
        memcpy(req, &fm, sizeof(filemsg));
        strcpy(req + sizeof(filemsg), filename.c_str());
        work_chan->cwrite(req, len);

        __int64_t file_size;
        work_chan->cread(&file_size, sizeof(__int64_t));
        cout << "File size: " << file_size << " bytes" << endl;

        string outname = "received/" + filename;
        FILE* out = fopen(outname.c_str(), "wb");
        if (!out) {
            perror("Cannot open output file");
            delete[] req;
            return 1;
        }

        const int chunk = MAX_MESSAGE - sizeof(filemsg) - 1;
        __int64_t offset = 0;
        while (offset < file_size) {
            int bytes = (int)min((__int64_t)chunk, file_size - offset);
            filemsg part(offset, bytes);
            memcpy(req, &part, sizeof(filemsg));
            strcpy(req + sizeof(filemsg), filename.c_str());
            work_chan->cwrite(req, sizeof(filemsg) + filename.size() + 1);

            int n = work_chan->cread(buf, MAX_MESSAGE);
            fwrite(buf, 1, n, out);
            offset += n;
        }
        fclose(out);
        delete[] req;
        cout << "File received and saved as " << outname << endl;
    }


	
	// closing the channel    
    MESSAGE_TYPE quit = QUIT_MSG;
	if (work_chan != &control) {
		work_chan->cwrite(&quit, sizeof(MESSAGE_TYPE));
		delete work_chan;
	}
	control.cwrite(&quit, sizeof(MESSAGE_TYPE));
    return 0;
}
