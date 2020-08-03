#include <AIO/Search/SearchEngine.hpp>

#include <AIO/Network/FakeNetwork.hpp>

#include <spdlog/spdlog.h>

namespace AIO::Search
{
SearchEngine::SearchEngine(SearchOptions option)
    : option_(option), manager_(option.NumSearchThreads)
{
    networkBarrier_.Init(option_.NumEvalThreads);
    for (int threadId = 0; threadId < option_.NumEvalThreads; ++threadId)
    {
        std::unique_ptr<Network::Network> network;

        network = std::make_unique<Network::FakeNetwork>();

        evalThreads_.emplace_back(&SearchEngine::evalThread, this, threadId,
                                  std::move(network));
    }

    for (int threadId = 0; threadId < option_.NumSearchThreads; ++threadId)
    {
        searchThreads_.emplace_back(&SearchEngine::searchThread, this,
                                    threadId);
    }

    networkBarrier_.Wait();

    updateRoot(nullptr);

    spdlog::info("search engine initialize done");
}

SearchEngine::~SearchEngine()
{
    manager_.Terminate();
    for (auto& t : searchThreads_)
        if (t.joinable())
            t.join();

    runningEvalThread_ = false;

    for (auto& t : evalThreads_)
        if (t.joinable())
            t.join();
}

void SearchEngine::Search()
{
}

void SearchEngine::Play(Game::Point pt)
{
    mainBoard_.Play(pt);
}

Game::Point SearchEngine::GetBestMove() const
{
    const TreeNode* bestNode = getBestNode();

    if (bestNode == nullptr)
        return Game::PASS;

    return getBestNode()->action;
}

const TreeNode* SearchEngine::GetRoot() const noexcept
{
    return root_;
}

TreeNode* SearchEngine::getBestNode()
{
    return const_cast<TreeNode*>(std::as_const(*this).getBestNode());
}

const TreeNode* SearchEngine::getBestNode() const
{
    if (root_ == nullptr)
        return nullptr;

    int maxVisits = 0;
    TreeNode* bestChild = nullptr;

    for (TreeNode* tempNowNode = root_->mostLeftChildNode;
         tempNowNode != nullptr; tempNowNode = tempNowNode->rightSiblingNode)
    {
        const int visits = tempNowNode->visits.load();
        if (visits > maxVisits)
        {
            maxVisits = visits;
            bestChild = tempNowNode;
        }
    }

    return bestChild;
}

void SearchEngine::updateRoot(TreeNode* newNode)
{
    TreeNode* node;
    if (newNode == nullptr)
    {
        node = new TreeNode;
        node->color = mainBoard_.Opponent();
    }
    else
    {
        node = new TreeNode(std::move(*newNode));

        for (TreeNode* tempNowNode = node->mostLeftChildNode;
             tempNowNode != nullptr;
             tempNowNode = tempNowNode->rightSiblingNode)
            node->parentNode = node;
    }

    root_ = node;
}

void SearchEngine::initRoot()
{
}

void SearchEngine::evaluate(const Game::Board& state, Network::Tensor& policy,
                            float& value)
{
    EvalTask task;
    task.state = Network::StateToTensor(state);
    auto fut = task.task.get_future();

    {
        std::lock_guard<std::mutex> lock(evalMutex_);

        evalTasks_.emplace_back(std::move(task));
    }

    auto [predPolicy, predValue] = fut.get();

    policy = std::move(predPolicy);
    value = predValue;
}

void SearchEngine::evalThread(int threadId,
                              std::unique_ptr<Network::Network> network)
{
    spdlog::info("[eval thread {}] initialize start", threadId);
    network->Initialize(option_.WeightFileName);

    networkBarrier_.Done();
    spdlog::info("[eval thread {}] initialize ended", threadId);

    while (runningEvalThread_)
    {
        std::vector<Network::Tensor> inputs;
        std::vector<std::promise<NetEval>> results;

        std::size_t batchSize;

        {
            std::lock_guard<std::mutex> lock(evalMutex_);

            batchSize =
                std::min<std::size_t>(option_.BatchSize, evalTasks_.size());
            for (std::size_t batch = 0; batch < batchSize; ++batch)
            {
                EvalTask task = std::move(evalTasks_.front());
                evalTasks_.pop_front();

                inputs.emplace_back(std::move(task.state));
                results.emplace_back(std::move(task.task));
            }
        }

        if (batchSize == 0)
            continue;

        std::vector<Network::Tensor> policy(batchSize);
        Network::Tensor value(batchSize);

        network->Evaluate(inputs, policy, value);

        for (std::size_t batch = 0; batch < batchSize; ++batch)
        {
            results[batch].set_value(
                std::make_tuple(std::move(policy[batch]), value[batch]));
        }
    }

    spdlog::info("[eval thread {}] shutdown", threadId);
}

void SearchEngine::searchThread(int threadId)
{
    if (!manager_.WaitResume())
    {
        spdlog::info("[search thread {}] shutdown", threadId);
        return;
    }

    spdlog::info("[search thread {}] start searching loop", threadId);

    while (true)
    {
        if (manager_.GetState() != SearchState::SEARCHING)
        {
            manager_.AckPause();
            if (!manager_.WaitResume())
            {
                break;
            }
        }

        // Searching routine
        // TODO: Implement this
    }

    spdlog::info("[search thread {}] shutdown", threadId);
}
}  // namespace AIO::Search
