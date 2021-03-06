#include <AIO/Search/SearchEngine.hpp>

#include <iostream>

using namespace AIO;

using std::cin;
using std::cout;
using std::endl;

int main()
{
    using namespace std::chrono_literals;

    Search::SearchOptions option;
    option.Load("play.json");
    
    Search::SearchEngine engine(option);

    std::string str;

    cout << "Input Computer Color: ";
    std::getline(cin, str);
    const Game::StoneColor comColor = Game::ColorUtil::Str2Color(str);

    Game::Board board;

    const int notComFlag = (comColor == Game::P_BLACK ? 0 : 1);
    int turn = 1;
    while (!board.IsEnd())
    {
        cout << "Turn " << turn << endl;
        board.ShowBoard(cout, turn % 2 == notComFlag);

        Game::Point move;
        if (turn % 2 == notComFlag)
        {
            cout << "Your move: ";
            std::getline(cin, str);

            if (str == "undo")
            {
                board.Undo();
                board.Undo();
                engine.Undo();
                engine.Undo();

                turn -= 2;
                cout << endl;

                continue;
            }

            move = Game::PointUtil::Str2Point(str);

            if (!board.IsValid(move))
                move = Game::INVALID_MOVE;

            while (move == Game::INVALID_MOVE)
            {
                cout << "Your move: ";
                std::getline(cin, str);

                move = Game::PointUtil::Str2Point(str);

                if (!board.IsValid(move))
                    move = Game::INVALID_MOVE;
            }

            engine.DumpStats();
        }
        else
        {
            engine.Search();
            engine.DumpStats();

            move = engine.GetBestMove();
        }

        board.Play(move);
        engine.Play(move);

        cout << endl;
        ++turn;
    }

    cout << "Gibo: ";

    for (const auto p : board.GetHistory())
    {
        if (p == Game::PASS)
            continue;

        cout << Game::PointUtil::PointStr(p);
    }
    cout << '\n';

    cout << endl
         << "Winner: " << Game::ColorUtil::ColorStr(board.GetWinner()) << endl;
}
