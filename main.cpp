#include "raylib.h"
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>
#include <iostream>
using namespace std;

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 700
#define CARD_WIDTH 60
#define CARD_HEIGHT 90
#define MAX_PLAYERS_ROW 7
#define MAX_NAME_CHARS 9
#define MAX_NUM_BOTS_CHARS 2

static Sound fxButton;
static Texture2D buttonTex;
static Texture2D logoTex;
static Texture2D cardBackTexture;
static Texture2D dummyFront;

enum GameState { MENU, BETTING, DEALING, REVEALING, SHOW_RESULT, END_GAME };
GameState gameState = MENU;


class Card {
public:
    string suit;
    string rank;
    Texture2D texture;
    Rectangle srcRec;
    Rectangle destRec;
    bool revealed;
    float flipProgress;

    Card(string s, string r, Texture2D tex, Vector2 centerPos)
        : suit(move(s)), rank(move(r)), texture(tex), revealed(false), flipProgress(0.0f)
    {
        static const string ranks[] = { "A","2","3","4","5","6","7","8","9","10","J","Q","K" };
        int col = 0, row = 0;

        // Mapping rank to column
        for (int i = 0; i < 13; i++) {
            if (ranks[i] == rank) {
                col = i;
                break;
            }
        }

        // Mapping suit to row/offset
        if (suit == "Co") { row = 0; col += 0; }       // ♥ left
        else if (suit == "Ro") { row = 0; col += 13; } // ♦ right
        else if (suit == "Bich") { row = 1; col += 0; } // ♠ left
        else if (suit == "Tep") { row = 1; col += 13; } // ♣ right

        // Cut from 26 cols × 2 rows
        float cardW = (float)tex.width / 26.0f;
        float cardH = (float)tex.height / 2.0f;
        srcRec = { col * cardW, row * cardH, cardW, cardH };

        // Draw scale 
        float scale = 1.5f;
        float drawW = CARD_WIDTH * scale;
        float drawH = cardH / cardW * drawW;

        // Use center position, draw from center
        destRec = { centerPos.x, centerPos.y, drawW, drawH };
    }

    void Draw(Texture2D backTexture) {
        if (revealed || flipProgress > 0.5f) {
            DrawTexturePro(texture, srcRec, destRec,
                { destRec.width / 2, destRec.height / 2 },
                0.0f, WHITE);
        }
        else {
            DrawTexturePro(backTexture,
                { 0, 0, (float)backTexture.width, (float)backTexture.height },
                destRec, { destRec.width / 2, destRec.height / 2 },
                0.0f, WHITE);
        }
    }

    int GetPoint() const {
        if (rank == "A") return 1;
        if (rank == "J" || rank == "Q" || rank == "K" || rank == "10") return 10;
        return stoi(rank);
    }
};


class Player {
public:
    string name;
    vector<Card> hand;
    bool isBot;
    int money;
    int bet;
    Vector2 position;

    Player(string n, bool b, Vector2 pos, int m = 1000)
        : name(move(n)), isBot(b), money(m), bet(0), position(pos) {
    }

    int GetTotalPoints() const {
        int sum = 0;
        for (const auto& card : hand) sum += card.GetPoint();
        return (sum == 10 ? 10 : sum % 10);
    }
};

vector<Player> players;
float dealTimer = 0.0f;
float revealTimer = 0.0f;
int currentRevealIndex = 0;
Player* winner = nullptr;
bool showContinue = false;
static int playerBet = 50;
//
void ArrangePlayers() {
    int totalBots = players.size() - 1;
    int topRow = min(totalBots, MAX_PLAYERS_ROW);
    int bottomRow = totalBots - topRow;

    float spacingTop = SCREEN_WIDTH / (topRow + 1);
    float spacingBottom = bottomRow > 0 ? SCREEN_WIDTH / (bottomRow + 1) : 0;

    for (int i = 1; i <= totalBots; i++) {
        float x, y;
        if (i <= topRow) {
            x = spacingTop * i - CARD_WIDTH / 2;
            y = 100;
        }
        else {
            x = spacingBottom * (i - topRow) - CARD_WIDTH / 2;
            y = 250;
        }
        players[i].position = { x, y };
    }
    players[0].position = { SCREEN_WIDTH / 2.0f - (1.5f * CARD_WIDTH), 500 };
}

void StartGame(const string& playerName, int numBots) {
    gameState = BETTING;

    if (players.empty()) {
        dummyFront = LoadTexture("resources/card_front.png");
        cardBackTexture = LoadTexture("resources/card_back.png");

        players.emplace_back(playerName, false, Vector2{ 0.0f, 0.0f });
        for (int i = 0; i < numBots; i++)
            players.emplace_back("Bot " + to_string(i + 1), true, Vector2{ 0.0f, 0.0f });

        ArrangePlayers();
    }

    vector<string> ranks = { "A","2","3","4","5","6","7","8","9","10","J","Q","K" };
    vector<string> suits = { "Co","Ro","Tep","Bich" };
    vector<pair<string, string>> deck;
    for (auto& s : suits)
        for (auto& r : ranks)
            deck.emplace_back(s, r);

    shuffle(deck.begin(), deck.end(), default_random_engine(GetRandomValue(1, 100000)));

    int cardIndex = 0;
    for (auto& p : players) {
        p.hand.clear();
        for (int i = 0; i < 3; i++) {
            string suit = deck[cardIndex].first;
            string rank = deck[cardIndex].second;
            cardIndex++;
            float totalWidth = 3 * CARD_WIDTH + 2 * 5;
            float offsetX = p.position.x - totalWidth / 2;
            Vector2 pos = { offsetX + i * (CARD_WIDTH + 5), p.position.y };
            p.hand.emplace_back(suit, rank, dummyFront, pos);
        }
    }

    dealTimer = 0.0f;
    revealTimer = 0.0f;
    currentRevealIndex = 0;
    winner = nullptr;
    showContinue = false;
}

void DrawBetting() {
    ClearBackground(DARKGREEN);
    DrawText("Chinh so tien dat cuoc bang cach an nut len xuong", SCREEN_WIDTH / 2 - 220, 50, 20, GOLD);

    if (IsKeyPressed(KEY_UP)) playerBet += 10;
    if (IsKeyPressed(KEY_DOWN)) playerBet = max(10, playerBet - 10);

    DrawText(("Your Bet: $" + to_string(playerBet)).c_str(), SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT / 2, 20, WHITE);

    for (auto& p : players) {
        DrawText(p.name.c_str(), p.position.x, p.position.y + CARD_HEIGHT + 5, 16, WHITE);
        DrawText(("$" + to_string(p.money)).c_str(), p.position.x, p.position.y + CARD_HEIGHT + 25, 16, YELLOW);
    }

    if (IsKeyPressed(KEY_ENTER)) {
        if (playerBet > players[0].money) playerBet = players[0].money;
        players[0].bet = playerBet;
        players[0].money -= playerBet;

        for (size_t i = 1; i < players.size(); i++) {
            int botBet = GetRandomValue(10, min(100, players[i].money));
            players[i].bet = botBet;
            players[i].money -= botBet;
        }

        for (auto& c : players[0].hand) c.revealed = true;

        gameState = DEALING;
    }
}

void DrawGame() {
    ClearBackground(DARKGREEN);
    for (auto& p : players) {
        for (auto& c : p.hand) c.Draw(cardBackTexture);
        DrawText(p.name.c_str(), p.position.x, p.position.y + CARD_HEIGHT + 5, 16, WHITE);
        DrawText(('$' + to_string(p.money)).c_str(), p.position.x, p.position.y + CARD_HEIGHT + 25, 16, YELLOW);
    }
}

void UpdateDealing() {
    dealTimer += GetFrameTime();
    if (dealTimer > 2.0f) gameState = REVEALING;
}

void UpdateRevealing() {
    revealTimer += GetFrameTime();
    if (revealTimer > 1.0f) {
        if (currentRevealIndex + 1 < players.size()) {
            currentRevealIndex++;
            for (auto& c : players[currentRevealIndex].hand)
                c.revealed = true;
            revealTimer = 0.0f;
        }
        else {
            gameState = SHOW_RESULT;
        }
    }
}

void FindWinner() {
    winner = &players[0];
    for (size_t i = 1; i < players.size(); i++) {
        if (players[i].GetTotalPoints() > winner->GetTotalPoints()) {
            winner = &players[i];
        }
    }
    int pot = 0;
    for (auto& p : players) pot += p.bet;
    winner->money += pot;
}

void DrawResult() {
    if (!winner) FindWinner();
    DrawText((winner->name + " wins!").c_str(), SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2, 30, GOLD);
    showContinue = true;
}
//
int main() {
    const int W = 800, H = 600;
    InitWindow(W, H, "Bai 3 Cay - Loading");
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
        DrawRectangle(100, H / 2 + 20, (int)((W - 200) * loadingProgress), 30, GREEN);
        DrawRectangleLines(100, H / 2 + 20, W - 200, 30, WHITE);
        DrawText(loadingText, W / 2 - loadingTextWidth / 2, H / 2 - 15, 30, WHITE);
        int pct = (int)(loadingProgress * 100 + 0.5f); if (pct > 100) pct = 100;
        char buf[16]; sprintf_s(buf, sizeof(buf), "%d%%", pct);
        DrawText(buf, W / 2 - 20, H / 2 + 60, 20, WHITE);
        EndDrawing();
        if (loadingComplete) this_thread::sleep_for(chrono::milliseconds(500));
    }

    // --- Load GUI assets ---
    fxButton = LoadSound("resources/buttonfx.wav");
    buttonTex = LoadTexture("resources/buttonFIX.png");
    Image logoImg = LoadImage("resources/logo.png");
    ImageResize(&logoImg, 450, 500);
    logoTex = LoadTextureFromImage(logoImg);
    UnloadImage(logoImg);

    bool exitWindowRequested = false;   // Flag to request window to exit
    bool exitWindow = false; 
    const int NUM_FRAMES = 3;
    float frameHeight = (float)buttonTex.height / NUM_FRAMES;
    float padBot = 20.0f;

    Rectangle sourceRec = { 0.0f,0.0f, (float)buttonTex.width, frameHeight };
    Rectangle btnRec = { (float)W / 2 - buttonTex.width / 2, (float)H - frameHeight - padBot, (float)buttonTex.width, frameHeight };
    Rectangle nameRec = { (float)W / 2 - 225.0f / 2, 300.0f, 225.0f, 50.0f };
    Rectangle botsRec = { (float)W / 2 - 225.0f / 2, 400.0f, 225.0f, 50.0f };

    char nameBuf[MAX_NAME_CHARS + 1] = "\0";
    char botsBuf[MAX_NUM_BOTS_CHARS + 1] = "\0";
    int nameLen = 0, botsLen = 0;
    bool showNameError = false, showBotsError = false;
    int errT = 0;
    bool btnDown = false, fadeOut = false;
    float fadeA = 0.0f;
    enum Focus { FN, FB } focus = FN;

    // --- Menu nhập tên + số bots ---
    while (!WindowShouldClose()) {
        Vector2 mp = GetMousePosition();
        int bs = CheckCollisionPointRec(mp, btnRec) ? (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? 2 : 1) : 0;
        sourceRec.y = bs * frameHeight;

        if (bs > 0 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            btnDown = true;
            PlaySound(fxButton);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mp, nameRec)) { focus = FN; showNameError = false; errT = 0; }
            else if (CheckCollisionPointRec(mp, botsRec)) { focus = FB; showBotsError = false; errT = 0; }
        }

        int key = GetCharPressed();
        if (focus == FN) {
            while (key > 0) {
                if ((key >= 32 && key <= 125) && nameLen < MAX_NAME_CHARS) {
                    nameBuf[nameLen++] = (char)key;
                    nameBuf[nameLen] = '\0'; showNameError = false;
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
                    botsBuf[botsLen] = '\0'; showBotsError = false;
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && botsLen > 0)
                botsBuf[--botsLen] = '\0';
        }

        if (btnDown) {
            bool ok = true;
            if (nameLen == 0) { ok = false; showNameError = true; }
            if (botsLen == 0) { ok = false; showBotsError = true; }
            if (ok) {
                try {
                    int nb = stoi(botsBuf);
                    if (nb < 0 || nb > 13) throw 0;
                }
                catch (...) { ok = false; showBotsError = true; }
            }
            if (ok) fadeOut = true;
            btnDown = false;
        }
        if (fadeOut) {
            fadeA += GetFrameTime() * 2.0f;
            if (fadeA > 1.0f) fadeA = 1.0f;
        }
        if (showNameError || showBotsError) {
            if (++errT > 120) { showNameError = showBotsError = false; errT = 0; }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawTexture(logoTex, 190, -100, WHITE);
        DrawRectangleLinesEx(nameRec, 2, showNameError ? RED : DARKGRAY);
        DrawRectangleLinesEx(botsRec, 2, showBotsError ? RED : DARKGRAY);
        DrawText("Player Name:", (int)(nameRec.x - 145), (int)(nameRec.y + 15), 20, DARKGRAY);
        DrawText("Number of Bots (0-13):", (int)(botsRec.x - 240), (int)(botsRec.y + 15), 20, DARKGRAY);
        DrawText(nameBuf, (int)(nameRec.x + 7), (int)(nameRec.y + 10), 40, MAROON);
        DrawText(botsBuf, (int)(botsRec.x + 7), (int)(botsRec.y + 10), 40, MAROON);
        if (showNameError) DrawText("Please enter name!", 520, 320, 20, RED);
        if (showBotsError) DrawText("Please enter number bots!", 520, 430, 20, RED);
        DrawTextureRec(buttonTex, sourceRec, Vector2{ btnRec.x, btnRec.y }, WHITE);
        if (fadeOut) {
            DrawRectangleRec(Rectangle{ 0.0f, 0.0f, (float)W, (float)H }, Fade(WHITE, fadeA));
        }
        EndDrawing();

        if (fadeOut && fadeA >= 1.0f) break;
    }

    CloseAudioDevice();
    CloseWindow();

    // --- Start GUI Gameplay ---
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Bai 3 Cay - Gameplay");
    SetTargetFPS(60);
    StartGame(string(nameBuf), stoi(string(botsBuf)));

    while (!WindowShouldClose()) {
        BeginDrawing();
        switch (gameState) {
        case BETTING:
            DrawBetting();
            break;
        case DEALING:
            UpdateDealing();
            DrawGame();
            break;
        case REVEALING:
            UpdateRevealing();
            DrawGame();
            break;
        case SHOW_RESULT:
            DrawGame();
            DrawResult();
            if (showContinue && IsKeyPressed(KEY_ENTER)) {
                StartGame(players[0].name, players.size() - 1);
            }
            break;
        case END_GAME:
            ClearBackground(BLACK);
            DrawText("Game Over!", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2, 40, RED);
            break;
        }
        EndDrawing();
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
            DrawRectangle(220, 240, 600, 100, BLACK);
            DrawText("BAN CO CHAC MUON DONG ? [Y/N]", 250, 280, 30, WHITE);
        }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
