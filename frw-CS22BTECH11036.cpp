#include <iostream>
#include <math.h>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <semaphore.h>
#include <chrono>
#include <ctime>
#include <random>
#include <cmath>
#include <fcntl.h>
#include <sstream>
using namespace std;

default_random_engine generator;

int nw, nr, kw, kr, cs, rem;
sem_t read_write_lock;
sem_t read_lock;
sem_t queue_lock;
sem_t print_lock;
int read_count = 0;
ofstream outputFile;

string getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch() % std::chrono::seconds(1));

    char buffer[20]; // Sufficient size to hold both time and microseconds
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", std::localtime(&now_c));

    // Append microseconds to the buffer
    std::string result = buffer;
    result += ":";
    result += std::to_string(microseconds.count());

    return result;
}

std::string getOrdinal(int n)
{
    if (n % 100 >= 11 && n % 100 <= 13)
    {
        return std::to_string(n) + "th";
    }
    switch (n % 10)
    {
    case 1:
        return std::to_string(n) + "st";
    case 2:
        return std::to_string(n) + "nd";
    case 3:
        return std::to_string(n) + "rd";
    default:
        return std::to_string(n) + "th";
    }
}

void printOutput(const string &output)
{
    outputFile << output << endl;
}

long long stringTolonglong(const std::string &timeString)
{
    std::istringstream iss(timeString);
    std::string token;
    std::vector<std::string> tokens;
    while (std::getline(iss, token, ':'))
    {
        tokens.push_back(token);
    }

    long long hours = std::stod(tokens[0]);
    long long minutes = std::stod(tokens[1]);
    long long seconds = std::stod(tokens[2]);
    long long microseconds = std::stod(tokens[3]);

    long long totalTimeMicroseconds = hours * 3600LL * 1000000LL + minutes * 60LL * 1000000LL + seconds * 1000000LL + microseconds;

    return totalTimeMicroseconds;
}

void reader(vector<long long> &read_req_time, vector<long long> &read_entry_time, int id)
{
    long long reqtime = 0;
    long long entrytime = 0;
    string time_req;
    string time_entry;
    for (int var = 0; var < kr; var++)
    {
        sem_wait(&print_lock);
        time_req = getCurrentTime();
        reqtime = reqtime + stringTolonglong(time_req);
        printOutput(getOrdinal(var + 1) + " time " + "CS Request by Reader Thread " + to_string(id) + " at " + time_req);
        sem_post(&print_lock);

        sem_wait(&queue_lock);
        sem_wait(&read_lock);
        read_count++;
        if (read_count == 1)
            sem_wait(&read_write_lock);
        sem_post(&queue_lock);
        sem_post(&read_lock);

        /*Critical Section*/
        sem_wait(&print_lock);
        time_entry = getCurrentTime();
        entrytime = entrytime + stringTolonglong(time_entry);
        printOutput(getOrdinal(var + 1) + " time " + "CS Entry by Reader Thread " + to_string(id) + " at " + time_entry);
        sem_post(&print_lock);

        // Simulate reading from CS
        exponential_distribution<double> delay_cs(1.0 / cs);
        auto delay = delay_cs(generator);
        this_thread::sleep_for(chrono::milliseconds((int)delay));

        sem_wait(&print_lock);
        printOutput(getOrdinal(var + 1) + " time " + "CS Exit by Reader Thread " + to_string(id) + " at " + getCurrentTime());
        sem_post(&print_lock);

        sem_wait(&read_lock);
        read_count--;
        if (read_count == 0)
            sem_post(&read_write_lock);
        sem_post(&read_lock);

        // Simulate remainder section
        exponential_distribution<double> delay_rem(1.0 / rem);
        delay = delay_rem(generator);
        this_thread::sleep_for(chrono::milliseconds((int)delay));
    }
    read_req_time[id] = (float) reqtime / kr;
    read_entry_time[id] = (float) entrytime / kr;
}

void writer(vector<long long> &write_req_time, vector<long long> &write_entry_time, int id)
{
    long long reqtime = 0;
    long long entrytime = 0;
    string time_req;
    string time_entry;
    for (int var = 0; var < kw; var++)
    {
        sem_wait(&print_lock);
        time_req = getCurrentTime();
        reqtime = reqtime + stringTolonglong(time_req);
        printOutput(getOrdinal(var + 1) + " time " + "CS Request by Writer Thread " + to_string(id) + " at " + time_req);
        sem_post(&print_lock);

        sem_wait(&queue_lock);
        sem_wait(&read_write_lock);
        sem_post(&queue_lock);

        /*Critical Section*/
        sem_wait(&print_lock);
        time_entry = getCurrentTime();
        entrytime = entrytime + stringTolonglong(time_entry);
        printOutput(getOrdinal(var + 1) + " time " + "CS Entry by Writer Thread " + to_string(id) + " at " + time_entry);
        sem_post(&print_lock);

        // Simulate writing in CS
        exponential_distribution<double> delay_cs(1.0 / cs);
        auto delay = delay_cs(generator);
        this_thread::sleep_for(chrono::milliseconds((int)delay));

        sem_wait(&print_lock);
        printOutput(getOrdinal(var + 1) + " time " + "CS Exit by Writer Thread " + to_string(id) + " at " + getCurrentTime());
        sem_post(&print_lock);

        sem_post(&read_write_lock);

        // Simulate remainder section
        exponential_distribution<double> delay_rem(1.0 / rem);
        delay = delay_rem(generator);
        this_thread::sleep_for(chrono::milliseconds((int)delay));
    }
    write_req_time[id] = (float) reqtime / kw;
    write_entry_time[id] = (float) entrytime / kw;
}

int main()
{
    ifstream myfile("inp-params.txt");
    if (!myfile.is_open())
    {
        cout << "Error opening input file" << endl;
        return 1;
    }
    myfile >> nw;
    myfile >> nr;
    myfile >> kw;
    myfile >> kr;
    myfile >> cs;
    myfile >> rem;

    outputFile.open("output-fair.txt");
    if (!outputFile.is_open())
    {
        cout << "Error opening output file" << endl;
        return 1;
    }

    vector<long long> write_req_time(nw);
    vector<long long> write_entry_time(nw);
    vector<long long> read_req_time(nr);
    vector<long long> read_entry_time(nr);

    // Initialize semaphores
    sem_init(&read_write_lock, 0, 1); // Initialize read-write lock semaphore with initial value 1
    sem_init(&read_lock, 0, 1);       // Initialize read lock semaphore with initial value 1
    sem_init(&queue_lock, 0, 1);      // Initialize queue lock semaphore with initial value 1
    sem_init(&print_lock, 0, 1);      // Initialize print lock semaphore with initial value 1

    vector<thread> writerthreads;
    for (int i = 0; i < nw; i++)
    {
        writerthreads.push_back(thread(writer, ref(write_req_time), ref(write_entry_time), i));
    }

    vector<thread> readerthreads;
    for (int i = 0; i < nr; i++)
    {
        readerthreads.push_back(thread(reader, ref(read_req_time), ref(read_entry_time), i));
    }

    for (auto &wth : writerthreads)
    {
        wth.join();
    }

    for (auto &rth : readerthreads)
    {
        rth.join();
    }
    outputFile.close();

    outputFile.open("time-fair.txt");
    if (!outputFile.is_open())
    {
        cout << "Error opening output file" << endl;
        return 1;
    }
    long long readers_avg = 0L;
    long long readers_worst = 0L;
    for (int i = 0; i < nr; i++)
    {
        readers_avg = readers_avg + read_entry_time[i] - read_req_time[i];
        readers_worst = max(readers_worst, (read_entry_time[i] - read_req_time[i]));
        outputFile << "Average Time of Reader Thread " << i << " " << read_entry_time[i] - read_req_time[i] << endl;
    }
    readers_avg = (float) readers_avg / nr;
    outputFile << "Average Time of Reader Threads " << readers_avg << endl;
    outputFile << "Worst Case Time of Reader Threads " << readers_worst << endl;
    outputFile << endl;

    long long writers_avg = 0L;
    long long writers_worst = 0L;
    for (int i = 0; i < nw; i++)
    {
        writers_avg = writers_avg + write_entry_time[i] - write_req_time[i];
        writers_worst = max(writers_worst, (write_entry_time[i] - write_req_time[i]));
        outputFile << "Average Time of Writer Thread " << i << " " << write_entry_time[i] - write_req_time[i] << endl;
    }
    writers_avg = (float) writers_avg / nw;
    outputFile << "Average Time of Writer Threads " << writers_avg << endl;
    outputFile << "Worst Case Time of Writer Threads " << writers_worst << endl;
    outputFile << endl;

    outputFile.close();
    return 0;
}

// clang++ -std=c++11 assgn.cpp
