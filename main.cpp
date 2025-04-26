// main.cpp

#include "raylib.h"
#include <string>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <vector>
#include <algorithm>
#include <random>
#include <cctype>
#include <iostream>
#include <memory>
#include <chrono>
#include <thread> 
using namespace std;

// =======================================
// GUI globals
// =======================================
static Sound     fxButton;
static Texture2D buttonTex;
static Texture2D logoTex;

#define MAX_NAME_CHARS     9
#define MAX_NUM_BOTS_CHARS 2

// =======================================
// Console‐mode classes
// =======================================
namespace Console {

    class Card {
        string suit, rank;
    public:
        Card(string s, string r) : suit(move(s)), rank(move(r)) {}
        int getPoint() const {
            if (rank == "A") return 1;
            if (rank == "J" || rank == "Q" || rank == "K" || rank == "10") return 10;
            return stoi(rank);
        }
        int getSuitPriority() const {
            if (suit == "Bich") return 4;
            if (suit == "Co")   return 3;
            if (suit == "Ro")   return 2;
            return 1;
        }
        void Display() const { cout << rank << " " << suit; }
    };

    class Deck {
        vector<Card> cards;
    public:
        Deck() { createDeck(); }
        void createDeck() {
            static const string suits[] = { "Co","Ro","Tep","Bich" };
            static const string ranks[] = { "A","2","3","4","5","6","7","8","9","10","J","Q","K" };
            cards.clear();
            for (auto& s : suits) for (auto& r : ranks) cards.emplace_back(s, r);
            Shuffle();
        }
        void Shuffle() {
            shuffle(cards.begin(), cards.end(), mt19937(random_device()()));
        }
        Card draw() {
            if (cards.empty()) createDeck();
            Card c = cards.back();
            cards.pop_back();
            return c;
        }
    };

    class Player {
        string name;
        vector<Card> hand;
        int money, bet;
        bool isBot;
    public:
        Player(string n, int m, bool b) : name(move(n)), money(m), bet(0), isBot(b) {}
        void resetHand() { hand.clear(); }
        void placeBet() {
            if (isBot) {
                bet = rand() % (money / 2) + 10;
                cout << name << " Bets : " << bet << "$\n";
            }
            else {
                cout << name << " Has " << money << "$. Enter bet: ";
                while (true) {
                    string in; cin >> in;
                    if (!all_of(in.begin(), in.end(), ::isdigit)) {
                        cout << "Invalid! Enter number: "; continue;
                    }
                    bet = stoi(in);
                    if (bet <= 0 || bet > money) {
                        cout << "Invalid! Re-enter: "; continue;
                    }
                    break;
                }
            }
            money -= bet;
        }
        void draw(Deck& d) { hand.push_back(d.draw()); }
        void showHand() const {
            cout << name << " Flip: ";
            for (auto& c : hand) { c.Display(); cout << " | "; }
            cout << "\n";
        }
        int getTotalPoints() const {
            int tot = 0; for (auto& c : hand) tot += c.getPoint();
            return tot == 10 ? 10 : tot % 10;
        }
        Card getHighestCard() const {
            return *max_element(hand.begin(), hand.end(),
                [](auto& a, auto& b) {
                    return a.getSuitPriority() < b.getSuitPriority();
                });
        }
        const string& getName() const { return name; }
        int getBet() const { return bet; }
        void addMoney(int x) { money += x; }
        int getMoney() const { return money; }
        bool isBankrupt() const { return money <= 0; }
    };

    class Game {
        Deck deck;
        vector<Player> players;
    public:
        Game(const string& playerName, int numberBots) {
            players.emplace_back(playerName, 1000, false);
            for (int i = 1; i <= numberBots; i++)
                players.emplace_back("Bot " + to_string(i), 1000, true);
        }
        void startGame() {
            deck.Shuffle();
            for (auto& p : players) { p.resetHand(); p.placeBet(); }
            for (int i = 0; i < 3; i++)
                for (auto& p : players) p.draw(deck);
        }
        void revealHands() {
            for (size_t i = 0; i < players.size(); i++) {
                cout << players[i].getName() << " Waiting...\n";
                this_thread::sleep_for(chrono::seconds(i == 0 ? 0 : 2));
                players[i].showHand();
                cout << "Points: " << players[i].getTotalPoints() << "\n"
                    << "----------------------\n";
            }
        }
        void findWinner() {
            Player* win = &players[0];
            for (size_t i = 1; i < players.size(); i++) {
                int p1 = win->getTotalPoints(), p2 = players[i].getTotalPoints();
                if (p2 > p1 ||
                    (p2 == p1 && players[i].getHighestCard().getSuitPriority()
            > win->getHighestCard().getSuitPriority()))
                    win = &players[i];
            }
            int pot = 0; for (auto& p : players) pot += p.getBet();
            win->addMoney(pot);
            cout << "Player " << win->getName()
                << " wins " << win->getTotalPoints()
                << " points! Gains " << pot
                << "$, now has " << win->getMoney() << "$\n";
        }
        void checkPlayers() {
            if (players[0].isBankrupt()) {
                cout << "You ran out of money! Game Over.\n"; exit(0);
            }
            players.erase(remove_if(players.begin() + 1, players.end(),
                [](auto& p) { return p.isBankrupt(); }),
                players.end());
            if (players.size() == 1) {
                cout << players[0].getName()
                    << " is last with money. Game Over.\n";
                exit(0);
            }
        }
    };

} // namespace Console

// =======================================
// runConsoleGame definition (global)
// =======================================
void runConsoleGame(const string& playerName, int numBots) {
    // clear terminal
#if defined(_WIN32)
    system("cls");
#else
    system("clear");
#endif

    Console::Game game(playerName, numBots);
    while (true) {
        game.startGame();
        game.revealHands();
        game.findWinner();
        game.checkPlayers();

        int choice;
        cout << "\nContinue? 1=Yes   2=No : ";
        while (!(cin >> choice) || (choice != 1 && choice != 2)) {
            cin.clear(); cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Enter 1 or 2: ";
        }
        if (choice == 2) break;
    }
}

// =======================================
// main()
// =======================================
int main() {
    const int W = 800, H = 450;

    // Initialize Raylib
    InitWindow(W, H, "Bai Cao");
    InitAudioDevice();
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    // --- Loading screen ---
    bool loadingComplete = false;
    float loadingProgress = 0.0f;
    const char* loadingText = "Loading...";
    int loadingTextWidth = MeasureText(loadingText, 30);

    while (!loadingComplete && !WindowShouldClose()) {
        loadingProgress += GetFrameTime() * 0.5f;
        if (loadingProgress >= 1.0f) {
            loadingProgress = 1.0f;
            loadingComplete = true;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        DrawRectangle(100, H / 2 + 20,
            (int)((W - 200) * loadingProgress),
            30, GREEN);
        DrawRectangleLines(100, H / 2 + 20, W - 200, 30, WHITE);

        DrawText(loadingText,
            W / 2 - loadingTextWidth / 2,
            H / 2 - 15, 30, WHITE);

        int percent = (int)(loadingProgress * 100 + 0.5f);
        if (percent > 100) percent = 100;
        char progressText[16];
        sprintf_s(progressText, sizeof(progressText), "%d%%", percent);
        DrawText(progressText,
            W / 2 - 20,
            H / 2 + 60, 20, WHITE);

        EndDrawing();

        if (loadingComplete)
            this_thread::sleep_for(chrono::milliseconds(500));
    }

    // --- Load GUI assets ---
    fxButton = LoadSound("resources/buttonfx.wav");
    buttonTex = LoadTexture("resources/buttonFIX.png");
    Image logoImg = LoadImage("resources/logo.png");
    ImageResize(&logoImg, 450, 500);
    logoTex = LoadTextureFromImage(logoImg);
    UnloadImage(logoImg);

    // UI setup
    const int NUM_FRAMES = 3;
    float frameHeight = (float)buttonTex.height / NUM_FRAMES;
    float paddingBot = 20.0f;

    Rectangle sourceRec = {
        0.0f, 0.0f,
        (float)buttonTex.width, frameHeight
    };
    Rectangle btnRec = {
        (float)W / 2 - buttonTex.width / 2,
        (float)H - frameHeight - paddingBot,
        (float)buttonTex.width,
        frameHeight
    };
    Rectangle nameRec = {
        (float)W / 2 - 225.0f / 2,
        180.0f,
        225.0f,
        50.0f
    };
    Rectangle botsRec = {
        (float)W / 2 - 225.0f / 2,
        260.0f,
        225.0f,
        50.0f
    };

    // Input buffers & state
    char nameBuf[MAX_NAME_CHARS + 1] = "\0";
    char botsBuf[MAX_NUM_BOTS_CHARS + 1] = "\0";
    int  nameLen = 0, botsLen = 0;
    bool showNameError = false, showBotsError = false;
    int  errorTimer = 0;
    const int ERROR_DURATION = 120;
    bool exitWindowRequested = false;
    bool exitWindow = false;
    bool btnPressed = false;
    bool fadingOut = false;
    float fadeAlpha = 0.0f;
    const float fadeSpeed = 2.0f;

    enum Focus { F_NAME, F_BOTS } focus = F_NAME;

    // --- Menu loop ---
    while (!WindowShouldClose()) {
        Vector2 mouseP = GetMousePosition();

        // Button hover & click
        int bs = CheckCollisionPointRec(mouseP, btnRec)
            ? (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 2 : 1)
            : 0;
        sourceRec.y = bs * frameHeight;
        if (bs > 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            btnPressed = true;
            PlaySound(fxButton);
        }

        // Focus & clear error
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mouseP, nameRec)) {
                focus = F_NAME; showNameError = false; errorTimer = 0;
            }
            else if (CheckCollisionPointRec(mouseP, botsRec)) {
                focus = F_BOTS; showBotsError = false; errorTimer = 0;
            }
        }

        // Text input
        int key = GetCharPressed();
        if (focus == F_NAME) {
            while (key > 0) {
                if ((key >= 32 && key <= 125) && nameLen < MAX_NAME_CHARS) {
                    nameBuf[nameLen++] = (char)key;
                    nameBuf[nameLen] = '\0';
                    showNameError = false;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && nameLen > 0)
                nameBuf[--nameLen] = '\0';
        }
        else {
            while (key > 0) {
                if (key >= '0' && key <= '9' && botsLen < MAX_NUM_BOTS_CHARS) {
                    botsBuf[botsLen++] = (char)key;
                    botsBuf[botsLen] = '\0';
                    showBotsError = false;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && botsLen > 0)
                botsBuf[--botsLen] = '\0';
        }

        // PLAY logic
        if (btnPressed) {
            bool valid = true;
            if (nameLen == 0) { valid = false; showNameError = true; }
            if (botsLen == 0) { valid = false; showBotsError = true; }
            int nb = 0;
            if (valid) {
                try {
                    nb = stoi(botsBuf);
                    if (nb < 0 || nb > 13) throw 0;
                }
                catch (...) {
                    valid = false; showBotsError = true;
                }
            }
            if (valid) fadingOut = true;
            btnPressed = false;
        }

        // Fade-out
        if (fadingOut) {
            fadeAlpha += fadeSpeed * GetFrameTime();
            if (fadeAlpha > 1.0f) fadeAlpha = 1.0f;
        }

        // Error timer
        if (showNameError || showBotsError) {
            if (++errorTimer > ERROR_DURATION) {
                showNameError = showBotsError = false;
                errorTimer = 0;
            }
        }

        // Draw GUI
        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawTexture(logoTex, 170, -150, WHITE);

        DrawRectangleLinesEx(nameRec, 2, showNameError ? RED : DARKGRAY);
        DrawRectangleLinesEx(botsRec, 2, showBotsError ? RED : DARKGRAY);
        DrawText("Player Name:", (int)(nameRec.x - 145), (int)(nameRec.y + 15), 20, DARKGRAY);
        DrawText("Number of Bots (0-13):", (int)(botsRec.x - 240), (int)(botsRec.y + 15), 20, DARKGRAY);
        DrawText(nameBuf, (int)(nameRec.x + 5), (int)(nameRec.y + 10), 40, MAROON);
        DrawText(botsBuf, (int)(botsRec.x + 5), (int)(botsRec.y + 10), 40, MAROON);
        if (showNameError) DrawText("Please enter name!", 520, 200, 20, RED);
        if (showBotsError) DrawText("Please enter number bots!", 520, 270, 20, RED);

        DrawTextureRec(buttonTex, sourceRec, Vector2{ btnRec.x, btnRec.y }, WHITE);

        if (fadingOut) {
            DrawRectangleRec(
                Rectangle{ 0.0f, 0.0f, (float)W, (float)H },
                Fade(WHITE, fadeAlpha)
            );
        }

        EndDrawing();

        if (fadingOut && fadeAlpha >= 1.0f) break;
    }
    while (!exitWindow)
    {
        // XAC NHAN KHI NAO AN NUT THOAT
        if (WindowShouldClose() || IsKeyPressed(KEY_ESCAPE)) exitWindowRequested = true;

        if (exitWindowRequested)
        {
            if (IsKeyPressed(KEY_Y)) exitWindow = true;
            else if (IsKeyPressed(KEY_N)) exitWindowRequested = false;
        }
        // Draw
        BeginDrawing();
        ClearBackground(RAYWHITE);
        if (exitWindowRequested)
        {
            DrawRectangle(100, 140, 600, 100, BLACK);
            DrawText("BAN CO CHAC MUON DONG ? [Y/N]", 130, 180, 30, WHITE);
        }
        EndDrawing();
    }

    // Tear down GUI
    CloseWindow();
    CloseAudioDevice();

    // Run console game
    runConsoleGame(string(nameBuf), stoi(string(botsBuf)));
    return 0;
}
