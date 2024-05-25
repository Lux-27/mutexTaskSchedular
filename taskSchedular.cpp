#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <condition_variable>

// defining job type
using Job = void (*)();

// defining error type
using Error = void (*)(const std::exception &);

// Scheduler Class
class Scheduler
{
public:
    Scheduler(size_t size, Error error);
    Scheduler(size_t size, nullptr_t) = delete; // Prevent creation of Scheduler object with Nullptr as param
    Scheduler(const Scheduler &) = delete;      // Delete copy constructor

    void schedule(Job j, long n);
    void wait();
    virtual ~Scheduler() = default; // Virtual Destructor for Scheduler class

private:
    std::condition_variable condition;
    std::mutex mutex;
    size_t maxJobsAllowed;
    const Error error;
    size_t countJobs{};
};

Scheduler::Scheduler(size_t size, const Error error) : maxJobsAllowed(size), error(error)
{
    if (error == nullptr)
    {
        throw std::runtime_error("non-null error callback required");
    }
}

void Scheduler::schedule(const Job j, long n)
{
    // unique lock ensures the critical section of a job is protected, and only one thread can execute this part of a program at any given time
    std::unique_lock<std::mutex> lock(this->mutex);

    // waits till count of all jobs running is less than maximum allowed Jobs
    condition.wait(lock, [this]
                   { return this->countJobs < this->maxJobsAllowed; });
    countJobs++; // one more job curring currently so increase count

    // shared pointer to the job, ensures that "job" object remains valid during its execution
    // prevents invalidity of 'job' object when 'job' goes out of scope

    // TODO : simplify this by sending job directly inside the function
    auto job = std::make_shared<Job>(j);

    // create a thread for every job with params n (time), job, and the pointer to the schedular instance
    std::thread thread{
        [n, job, this]
        {
            // sleep for n milisec to simulate job scheduling delay
            std::this_thread::sleep_for(std::chrono::milliseconds(n));

            try
            {
                (*job)(); // job executed by de-referrencing the pointer to the job
            }
            catch (const std::exception &e)
            { // catch errors
                this->error(e);
            }
            catch (...)
            {
                this->error(std::runtime_error("Unknown"));
            }

            condition.notify_one(); // after job completion, notify the thread waiting for the job completion
            countJobs--;            // job execution completed to running jobs - 1
        }};
    thread.detach();
}

void Scheduler::wait()
{
    // unique lock ensures the critical section of a job is protected, and only one thread can execute this part of a program at any given time
    std::unique_lock<std::mutex> lock(this->mutex);

    // lambda function passed to wait which check if all jobs are done -- countJobs == 0
    condition.wait(lock, [this]
                   { return this->countJobs == 0; });
}

int main()
{
    auto start = std::chrono::high_resolution_clock::now();

    // create a schedular class with maxAllowedJobs = 2 alongwith an error function
    Scheduler scheduler(2, [](const std::exception &e)
                        { std::cout << "Error: " << e.what() << std::endl; });

    // Creating different job scenarios
    // 1. Normal job with 1000ms wait
    scheduler.schedule([]
                       { std::cout << 1 << std::endl; },
                       1000);

    // 2. Job with err, 150ms wait
    scheduler.schedule([]
                       {std::cout << 2 << std::endl; throw "err"; },
                       150);

    // 3. Job with out of range err, 1500 wait
    scheduler.schedule([]
                       {std::cout << 2 << std::endl; throw std::out_of_range("err"); },
                       1500);

    scheduler.schedule([]
                       { std::cout << 3 << std::endl; },
                       100);
    scheduler.schedule([]
                       { std::cout << 4 << std::endl; },
                       3000);

    scheduler.wait();

    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
    std::cout << "Waited: " << duration.count() << std::endl;
};
