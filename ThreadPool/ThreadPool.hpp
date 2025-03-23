#ifndef HSLL_THREADPOOL
#define HSLL_THREADPOOL

#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <condition_variable>

namespace HSLL
{
    /**
     * @brief Thread pool template class for managing and executing tasks concurrently
     * @tparam T Task type that can be executed
     */
    template <class T>
    class ThreadPool
    {
    private:
        bool flag;                 // Flag to control the life cycle of the thread pool
        unsigned int maxSize;      // Maximum number of tasks that can be queued

        std::mutex mtx;             // Mutex for protecting shared data
        std::queue<T> tasks;        // Queue for storing tasks
        std::condition_variable cv; // Condition variable for task synchronization
        std::vector<std::thread> threads; // Vector to hold worker threads

    public:
        /**
         * @brief Default constructor
         * Initializes the thread pool with default values
         */
        ThreadPool() : flag(true), maxSize(0) {}

        /**
         * @brief Initialize the thread pool
         * @param maxSize Maximum number of tasks that can be queued
         * @param threadNum Number of worker threads to create
         */
        void Init(unsigned int maxSize, unsigned int threadNum)
        {
            this->maxSize = maxSize;
            for (int i = 0; i < threadNum; i++)
                threads.emplace_back(std::thread(&ThreadPool::Worker, this));
        }

        /**
         * @brief Append a task to the thread pool
         * @param task Task to be added
         * @return true If the task is successfully appended
         * @return false If the task queue is full
         */
        bool Append(T &&task)
        {
            std::unique_lock<std::mutex> uLock(mtx);

            if (tasks.size() >= maxSize)
                return false;

            tasks.push(std::move(task));

            cv.notify_one();
            return true;
        }

        /**
         * @brief Worker function for executing tasks
         */
        void Worker()
        {
            while (flag)
            {
                T task;
                {
                    std::unique_lock<std::mutex> uLock(mtx);
                    cv.wait(uLock, [this]()
                            { return tasks.size() || !flag; });

                    if (!flag)
                        return;

                    task = std::move(tasks.front());
                    tasks.pop();
                }

                task.execute();
            }
        }

        /**
         * @brief Exit the thread pool
         * Stops all worker threads and cleans up resources
         */
        void Exit()
        {
            {
                std::unique_lock<std::mutex> uLock(mtx);
                flag = false;
            }
            cv.notify_all();

            for (auto &thread : threads)
            {
                if (thread.joinable())
                    thread.join();
            }
        }

        /**
         * @brief Destructor
         * Calls Exit() to clean up the thread pool
         */
        ~ThreadPool()
        {
            Exit();
        }

        // Disable copy constructor and assignment operator
        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;
    };
}

#endif // !HSLL_THREADPOOL