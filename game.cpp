// sorry, didn't feel like making classes... should probably have an easy CMake template

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <unistd.h>

using namespace std;

enum GameType {
    PLAYER_GUESS = 0,
    COMPUTER_GUESS = 1
};

enum ComputerType {
    TRUST = 0,
    NO_TRUST = 1
};

enum MatchType {
    MISS = 0, // not in word
    SLIDE = 1, // right letter, wrong position
    HIT = 2 // right letter, right position
};

using Hint = vector<MatchType>;

const int MAX_GUESSES = 6;
const string PROMPT = "> ";
bool GAME_WON = false;
int WORD_LENGTH = 0;
GameType GAME_TYPE = PLAYER_GUESS;
ComputerType COMPUTER_TYPE = TRUST;

unordered_set<string>         wordMap;
vector<string>                wordList;
unordered_map<char, double>   letterScoreMap;
unordered_map<string, double> wordScoreMap;

double calculateWordScore(const string& word)
{
    // sum values of each unique letter in the word

    unordered_set<char> foundLetters;
    foundLetters.reserve(word.size());

    double score = 0;

    for (char c : word) {
        if (foundLetters.count(c) > 0) continue;

        score += letterScoreMap[c];

        foundLetters.insert(c);
    }

    return score;
}

double getAdjustedWordScore(const unordered_set<char>& lettersInLastGuess, const string& word)
{
    // remove letters from word that have already been found
    string adjustedWord;

    for (char c : word) {
        if (0 == lettersInLastGuess.count(c)) {
            adjustedWord += c;
        }
    }

    return calculateWordScore(adjustedWord);
}

string getOptimalWord(const unordered_set<string>& activeWords, const string& lastGuess)
{
    // return word with highest letter score
    // adjust scores by removing letters that have already been found

    if (activeWords.empty()) {
        cout << "Oops, I don't know of any more words that match the hints." << endl;
        exit(0);
    }

    unordered_set<char> lettersInLastGuess;
    for (char c : lastGuess) {
        lettersInLastGuess.insert(c);
    }

    // choose a random word to start
    auto it = activeWords.begin();
    advance(it, rand() % activeWords.size());

    string optimalWord = *it;
    double optimalScore = getAdjustedWordScore(lettersInLastGuess, optimalWord);

    for (const auto& w : activeWords) {
        double score = getAdjustedWordScore(lettersInLastGuess, w);
        if (score > optimalScore) {
            optimalWord = w;
            optimalScore = score;
        }
    }

    return optimalWord;
}

void readWords(const string& file)
{
    ifstream infile(file);

    string line;
    while (getline(infile, line))
    {
        istringstream iss(line);

        string word;
        iss >> word;

        if (word.size() == WORD_LENGTH) {
            wordMap.insert(word);
            wordList.push_back(word);
            wordScoreMap[word] = calculateWordScore(word);
        }
    }
}

void readLetters(const string& file)
{
    // TODO: this should ideally be calculated based on the provided dictionary
    
    ifstream infile(file);

    string line;
    while (getline(infile, line))
    {
        istringstream iss(line);

        char letter;
        double freq;

        iss >> letter >> freq;

        letterScoreMap[tolower(letter)] = freq;
    }
}

bool checkHitLetters(const vector<pair<char, int> >& hitLetters, const string& word)
{
    for (const auto& v : hitLetters) {
        if (word[v.second] != v.first) {
            // letter in this word does not match the required letter
            return false;
        }
    }

    return true;
}

bool checkSlideLetters(const unordered_map<char, bool>& foundMapTemplate, const unordered_map<char, int>& slideLetters, const string& word)
{
    unordered_map<char, bool> foundMap = foundMapTemplate;
    // create a map for each letter which will be marked found or not
    for (size_t i = 0; i < word.size(); ++i) {
        char l = word[i];

        // also check that letter is not in the same position as last word (which is the wrong position if it's a slide letter)
        auto letterIt = slideLetters.find(l);
        if (letterIt != slideLetters.end()) {
            if (i == letterIt->second) {
                // don't want this word, it repeats the position of this letter
                return false;
            }
        }

        // mark this letter as found
        auto it = foundMap.find(l);
        if (it != foundMap.end()) {
            it->second = true;
        }
    }

    // are all slide letters present?
    for (auto& v : foundMap) {
        if (!v.second) {
            return false;
        }
    }

    return true;
}

bool checkMissLetters(const unordered_set<char>& missLetters, const string& word)
{
    // NOTE: at the moment, this method complicates dealing with duplicates
    // if word is 'game', and guess is 'gage', would like to return MISS for second G
    // but then would not be allowed to contain G

    for (const auto& l : word) {
        // letter cannot be in missLetters
        if (missLetters.count(l) > 0) {
            return true;
        }
    }

    return false;
}

void filterWordsByHint(unordered_set<string>& words, const string& lastGuess, const Hint& hint)
{
    // list of hit letters and their indices
    unordered_set<char> missLetters;
    unordered_map<char, int> slideLetters;
    vector<pair<char, int> > hitLetters;
    for (size_t i = 0; i < hint.size(); ++i) {
        auto h = hint[i];
        
        switch (h) {
            case MISS:
                missLetters.insert(lastGuess[i]);
                break;
            case SLIDE:
                slideLetters[lastGuess[i]] = i;
                break;
            case HIT:
                hitLetters.emplace_back(lastGuess[i], i);
                break;
        }
    }

    // keep track of whether slide letters were found for each word
    unordered_map<char, bool> foundMapTemplate;
    for (const auto& l : slideLetters) {
        foundMapTemplate[l.first] = false;
    }

    for (auto it = words.begin(); it != words.end(); /* NOTHING */ ) {
        string w = *it;

        // word should contain all HIT letters in correct position
        bool hasHitLetters = checkHitLetters(hitLetters, w);
        if (!hasHitLetters) {
            it = words.erase(it);
            continue;
        }

        // word should contain all SLIDE letters, in any position BUT their previous position
        bool hasSlideLetters = checkSlideLetters(foundMapTemplate, slideLetters, w);
        if (!hasSlideLetters) {
            it = words.erase(it);
            continue;
        }

        // word cannot contain any MISS letters
        bool hasMissLetters = checkMissLetters(missLetters, w);
        if (hasMissLetters) {
            it = words.erase(it);
            continue;
        }

        // word matches, keep it
        ++it;
    }
}

string makeGuessFromHint(unordered_set<string>& words, const string& lastGuess, const Hint& hint)
{
    // get a new list of possible words based on what contains those letters
    filterWordsByHint(words, lastGuess, hint);

    return getOptimalWord(words, lastGuess);
}

void showHint(const Hint& hint)
{
    cout << "      ";
    for (const auto& v : hint) {
        cout << v;
    }
    cout << "\n";
}

string getGuess(int guessNum)
{
    string guess("");

    while (true) {
        cin >> guess;

        if (guess.size() != WORD_LENGTH){
            cout << "Guess must be " << WORD_LENGTH << " letters." << endl;
            cout << "(" << guessNum << ") " << PROMPT;
            continue;
        } else if (wordMap.find(guess) == wordMap.end()) {
            cout << "Guess must be a real word." << endl;
            cout << "(" << guessNum << ") " << PROMPT;
            continue;
        }
        
        break;
    }

    return guess;
}

string getPlayerWord()
{
    cout << "Okay, please choose a " << WORD_LENGTH << "-letter word! I won't look..." << endl;

    string playerWord("");

    while (true) {
        cout << PROMPT;
        cin >> playerWord;
        if (playerWord.size() != WORD_LENGTH){
            cout << "Word must be " << WORD_LENGTH << " letters." << endl;
            continue;
        } else if (wordMap.find(playerWord) == wordMap.end()) {
            cout << "Sorry, I can't find that word in my dictionary." << endl;
            // TODO: "should I add it?"
            continue;
        }
        
        break;
    }

    return playerWord;
}

Hint getPlayerHint()
{
    Hint hint;
    
    string playerHint("");


    while (true) {
        cout << "    " << PROMPT;
        cin >> playerHint;

        if (playerHint.size() != WORD_LENGTH){
            cout << "Hint must be " << WORD_LENGTH << " numbers." << endl;
            continue;
        }

        bool valid = true;
        for (char c : playerHint) {
            try {
                int val = c - '0';
                if (val != 0 && val != 1 && val != 2) {
                    cout << "Values must be 0, 1, or 2" << endl;
                    valid = false;
                    break;
                }

                hint.push_back(static_cast<MatchType>(val));

            } catch (const std::invalid_argument& e) {
                cout << "Hint must contain only numbers." << endl;
                valid = false;
                break;
            }
        }

        if (valid) break;
        // else keep asking
    }

    return hint;
}

Hint checkGuess(const string& guess, 
                const string& word, 
                const unordered_set<char>& lettersInWord)
{
    Hint matches;
    matches.reserve(word.size());

    int i = 0;

    while (i < word.size())
    {
        if (guess[i] == word[i]) {
            matches.push_back(HIT);
        } else if (lettersInWord.find(guess[i]) != lettersInWord.end()) {
            matches.push_back(SLIDE);
        } else {
            matches.push_back(MISS);
        }

        ++i;
    }

    return matches;
}

void checkWin(const Hint& hint)
{
    bool won = true;

    for (const auto& match : hint) {
        if (match != HIT) {
            won = false;
            break;
        }
    }

    if (won) {
        GAME_WON = true;
    }
}

void playGame()
{
    // (COMPUTER_GUESS) will filter words based on hints
    auto activeWords = wordMap;

    string word, guess;
    Hint hint;

    if (GAME_TYPE == PLAYER_GUESS) {
        cout << "Okay, choosing a word...";
        
        size_t index = rand() % wordList.size();
        word = wordList[index];

        cout << " I chose a " << word.size() << "-letter word.\n";
        cout << "word=" << word << endl;
        cout << "Begin guessing -- you have six tries!\n";

    } else if (GAME_TYPE == COMPUTER_GUESS) {
        if (COMPUTER_TYPE == TRUST) {
            word = getPlayerWord();
        }
    }

    // collect letters from word
    unordered_set<char> lettersInWord;
    for (const auto& c : word) {
        lettersInWord.insert(c);
    }

    int guessNum = 0;
    while(guessNum++ < MAX_GUESSES && !GAME_WON)
    {
        cout << "(" << guessNum << ") " << PROMPT;

        // get guess, either player or computer
        switch (GAME_TYPE) {
            case PLAYER_GUESS:
                guess = getGuess(guessNum);
                break;
            case COMPUTER_GUESS:
                guess = makeGuessFromHint(activeWords, guess, hint);
                cout << guess << "\n";
                break;
        }

        // get hint, by input or calculation
        if (GAME_TYPE == COMPUTER_GUESS && COMPUTER_TYPE == NO_TRUST) {
            // ask player for hint
            hint = getPlayerHint();
        } else {
            hint = checkGuess(guess, word, lettersInWord);
            showHint(hint);
        }

        checkWin(hint);

        if (GAME_TYPE == COMPUTER_GUESS) sleep(1);
    }

    if (GAME_WON) {
        if (GAME_TYPE == PLAYER_GUESS) {
            cout << "Congratulations! You got it!" << endl;
        } else if (GAME_TYPE == COMPUTER_GUESS) {
            cout << "I guessed correctly!" << endl;
        }
    } else {
        if (GAME_TYPE == PLAYER_GUESS) {
            cout << "Better luck next time! The word was " << word << endl;
        } else if (GAME_TYPE == COMPUTER_GUESS) {
            cout << "You outsmarted me... I'll get you next time! (or try that word again, I may do better!)" << endl;
        }
    }
}

void showUsage(const string& name)
{
    cerr << "Usage: " << name << " WORD_LENGTH"
         << "\n\tWORD_LENGTH\tNumber of letters in word" 
         << endl;
}

int main(int argc, char** argv)
{
    // cout << "Press enter to continue.";
    // cin.ignore();

    if (argc != 2) {
        showUsage(argv[0]);
        exit(0);
    }

    try {
        WORD_LENGTH = stoi(argv[1]);
    }
    catch (const std::invalid_argument& e) {
        showUsage(argv[0]);
        exit(0);
    }

    // "partie", guessed "tarrie" twice in a row
    // need to mark "miss" for a second letter of a hit if there's only one

    srand(time(NULL));

    cout << "Welcome to Wordle! Reading the dictionary for all " << WORD_LENGTH << " letter words..." << endl;

    readLetters("letter_frequencies.txt");
    readWords("words_alpha.txt");

    cout << "Found " << wordList.size() << " words." << endl;

    if (wordList.empty()) {
        exit(1);
    }

    cout << "Rules:"
         << "\n- Enter any English word with " << WORD_LENGTH << " letters"
         << "\n- Hints after each guess look like 01102"
         << "\n\t- 0 the letter does not appear in the word"
         << "\n\t- 1 the letter appears in the word, but not in that position"
         << "\n\t- 2 the letter is in the correct position"
         << endl;

    // sleep(1);

    // string lastGuess = "trains";
    // Hint hint = { HIT, MISS, MISS, SLIDE, SLIDE, HIT };
    // filterWordsByHint(wordMap, lastGuess, hint);

    // for (auto& w : words) {
    //     cout << w << " ";
    // }
    // cout << endl;

    // exit(0);


    // sleep(1);

    cout << "Would you like to guess (0), or provide a word for me to guess (1)? ";
    int choice = 1;
    cin >> choice;
    GAME_TYPE = (choice) ? COMPUTER_GUESS : PLAYER_GUESS;

    if (GAME_TYPE == COMPUTER_GUESS) {
        cout << "Do you trust me enough to tell me the word, so I can guess on my own? I only use the word to generate a hint. [y/n] ";

        char computerChoice;
        cin >> computerChoice;
        computerChoice = tolower(computerChoice);

        COMPUTER_TYPE = (computerChoice == 'y') ? TRUST : NO_TRUST;

        cin.ignore(); // clear newline from previous input

        if (COMPUTER_TYPE == NO_TRUST) {
            cout << "Okay, you'll have to provide the hints to me after I guess, following the rules above. Press enter to continue.\n";
            cin.ignore();
        }
    }

    playGame();

    cout << "Thanks for playing!" << endl;
}