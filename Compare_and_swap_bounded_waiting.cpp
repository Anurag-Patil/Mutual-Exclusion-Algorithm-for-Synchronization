#include <iostream>
#include <atomic>
#include <fstream>
#include <thread>
#include <vector>
#include <string>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <random>

using namespace std;

float lambda1,lambda2;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

vector<bool> waiting;
vector<vector<vector<int>>> time_record;

atomic<bool> lock(false);

string getSysTime();

void initialize(int n, int k);
int get_time(string a);
void print(int n, int k);
float average_time(int n, int k);
float worst_time(int n, int k);

bool compare_and_swap();
void entry_sec_cas_bounded(int i,thread::id id, int k);
void exit_sec_cas_bounded(int i,thread::id id, int k, int n);
void cas_bounded_testcs(int n, int k, int j, double tt1, double tt2);
void cas_bounded(int n, int k);

ofstream fout;

int main()
{
	ifstream fin;
	fin.open("inp-params.txt");
	int n,k;
	fin>>n>>k>>lambda1>>lambda2;
	fin.close();

	for(int i = 0; i < n; i++)
	{
		waiting.push_back(false);
		vector<int> temp = {0,0};
		vector<vector<int>> temps;
		for(int j = 0; j < k; j++)
		{
			temps.push_back(temp);
		}
		time_record.push_back(temps);
	} 

	fout.open("cas-bounded-output.txt");

	cas_bounded(n,k);

	fout.close();

	fout.open("cas-bounded-stat.txt");

	fout<<"Bounded CAS Stat's :\n";
	fout<<"No. of thread created :"<<n<<'\n';
	fout<<"No. of times each thread will enter critical section :"<<k<<'\n';
	fout<<"The Average time taken by the process at a time to enter the CS is : "<<average_time(n,k)<<" seconds. \n";
	fout<<"The Worst time taken by the process at a time to enter the CS is : "<<worst_time(n,k)<<" seconds. \n";

	fout.close();

	return 0;
}

float average_time(int n, int k)
{
	float sum = 0;
	for(int i = 0; i < n; i++)
	{
		for(int j = 0; j < k; j++)
		{
			sum += (time_record[i][j][1] - time_record[i][j][0]);
		}
	}
	return (sum / (n * k));
}

float worst_time(int n, int k)
{
	float max = -1;
	for(int i = 0; i < n; i++)
	{
		for(int j = 0; j < k; j++)
		{
			if(max < (time_record[i][j][1] - time_record[i][j][0]))
			{
				max = (time_record[i][j][1] - time_record[i][j][0]);
			}
		}
	}
	return max;
}

void print(int n, int k)
{
	cout<<'\n';
	for(int i = 0; i < n; i++)
	{
		cout<<"ID :"<<i+1<<'\n';
		for(int j = 0; j < k; j++)
		{
			cout<<j+1<<" "<<time_record[i][j][0]<<" "<<time_record[i][j][1]<<"\t\t"<<(time_record[i][j][1] - time_record[i][j][0])<<'\n';
		}
		cout<<'\n';
	}
	cout<<'\n';
}

void initialize(int n, int k)
{
	for(int i = 0; i < n ; i++)
	{
		for(int j = 0; j < k; j++)
		{
			time_record[i][j][0] = 0;
			time_record[i][j][1] = 0;
		}
	}
}

string getSysTime()
{
	time_t current_time = time(NULL);
	string temp = ctime(&current_time);
	temp.pop_back();
	return temp;
}

bool compare_and_swap()
{
	pthread_mutex_lock(&mutex);
	bool temp = lock;
	if(lock == false)
	{
		lock = true;
	}
	pthread_mutex_unlock(&mutex);
	return temp;
}

void cas_bounded(int n, int k)
{
	default_random_engine generator1;
  	exponential_distribution<double> distribution1(lambda1);
  	default_random_engine generator2;
  	exponential_distribution<double> distribution2(lambda2);

	initialize(n,k);
	fout<<"\nBounded CAS ME Output:\n";
	vector <thread> th;
	for(int i = 0; i < n; i++)
	{
	  	double tt1 = distribution1(generator1);
	  	double tt2 = distribution2(generator2);
		th.push_back(thread(cas_bounded_testcs,n,k,i,tt1,tt2));
	}
	for(thread &t : th)
	{
		t.join();
	}
	lock = false;
}

void cas_bounded_testcs(int n, int k, int j, double tt1, double tt2)
{
	thread::id id = this_thread::get_id();
	useconds_t t1 = tt1 * 1000000;
	useconds_t t2 = tt2 * 1000000;
	for(int i = 0; i < k; i++)
	{
		 entry_sec_cas_bounded(i,id,j);
		 usleep(t1);
		 exit_sec_cas_bounded(i,id,j,n);
		 usleep(t2);
	}
}

void entry_sec_cas_bounded(int i, thread::id id, int k)
{
	pthread_mutex_lock(&mutex);
	string reqEnterTime = getSysTime();
	time_record[k][i][0] = get_time(reqEnterTime);
	fout<<i+1<<"th CS Request at "<<reqEnterTime<<" by thread "<<id<<"\n";
	pthread_mutex_unlock(&mutex);
	waiting[k] = true;
	bool key = true;
	while(waiting[k] && key)
	{
		key = compare_and_swap();
	}
	waiting[k] = false;
	pthread_mutex_lock(&mutex);
	string actEnterTime = getSysTime();
	time_record[k][i][1] = get_time(actEnterTime);
	fout<<i+1<<"th CS Entery at "<<actEnterTime<<" by thread "<<id<<"\n";
	pthread_mutex_unlock(&mutex);
}

void exit_sec_cas_bounded(int i, thread::id id, int k, int n)
{	
	int j = (k + 1) % n;
	while((j != k) && (!waiting[j]))
	{
		j = (j + 1) % n;
	}
	pthread_mutex_lock(&mutex);
	if(j == k)
	{
		lock = false;
	}
	else
	{
		waiting[j] = false;
	}
	string exitTime = getSysTime();
	fout<<i+1<<"th CS Exit at "<<exitTime<<" by thread "<<id<<"\n";
	pthread_mutex_unlock(&mutex);
}

int get_time(string s)
{
	int time_day = ((s[8] - 48) * 10) + (s[9] - 48);
	int time_hr = ((s[11] - 48) * 10) + (s[12] - 48);
	int time_min = ((s[14] - 48) * 10) + (s[15] - 48);
	int time_sec = ((s[17] - 48) * 10) + (s[18] - 48);
	int time_total = (time_day * 3600 * 24) + (time_hr * 3600) + (time_min * 60) + time_sec;
	return time_total;
}