#include <array>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

constexpr int kHoleCount = 9;
constexpr double kExplorationRate = 0.20;
constexpr int kMaxRounds = 15;
constexpr int kWhacksToWin = 3;
constexpr int kHintsPerGame = 2;

using SwingCounts = std::array<int, kHoleCount>;
using HoleProbabilities = std::array<double, kHoleCount>;

enum class MoleRule { Classic, Unpredictable };

std::string ruleName(MoleRule rule) {
    return rule == MoleRule::Classic ? "Classic" : "Unpredictable";
}

double laplaceEstimate(int swingsAtHole, int rounds) {
    return (swingsAtHole + 1.0) / (rounds + 2.0);
}

std::vector<int> leastSwungHoles(const SwingCounts& counts) {
    int lowestCount = counts[0];
    for (int count : counts) {
        if (count < lowestCount) lowestCount = count;
    }
    std::vector<int> holes;
    for (int hole = 1; hole <= kHoleCount; ++hole) {
        if (counts[hole - 1] == lowestCount) holes.push_back(hole);
    }
    return holes;
}

HoleProbabilities hidingDistribution(MoleRule rule, const SwingCounts& counts, int rounds) {
    HoleProbabilities probabilities{};
    if (rule == MoleRule::Classic) {
        std::vector<int> safest = leastSwungHoles(counts);
        for (int hole = 1; hole <= kHoleCount; ++hole) {
            probabilities[hole - 1] = kExplorationRate / kHoleCount;
        }
        for (int hole : safest) {
            probabilities[hole - 1] += (1.0 - kExplorationRate) / safest.size();
        }
    }
    else {
        double total = 0.0;
        for (int hole = 1; hole <= kHoleCount; ++hole) {
            probabilities[hole - 1] = 1.0 / laplaceEstimate(counts[hole - 1], rounds);
            total += probabilities[hole - 1];
        }
        for (double& probability : probabilities) probability /= total;
    }
    return probabilities;
}

void printPercentGrid(const HoleProbabilities& probabilities) {
    for (int row = 0; row < 3; ++row) {
        std::cout << "   ";
        for (int col = 0; col < 3; ++col) {
            int hole = row * 3 + col + 1;
            std::cout << " " << hole << ":" << std::setw(3)
                << static_cast<int>(probabilities[hole - 1] * 100 + 0.5) << "% ";
        }
        std::cout << "\n";
    }
}

class MoleAI {
public:
    MoleAI(unsigned int seed, MoleRule moleRule)
        : rule(moleRule), rng(seed), explorationRoll(kExplorationRate) {
    }

    double estimateSwingProbability(int hole) const {
        return laplaceEstimate(swingCounts[hole - 1], roundsObserved);
    }

    int chooseHidingHole() {
        isExploring = false;

        if (rule == MoleRule::Unpredictable) {
            HoleProbabilities probabilities = hidingDistribution(rule, swingCounts, roundsObserved);
            std::discrete_distribution<int> pickHole(probabilities.begin(), probabilities.end());
            int chosenHole = pickHole(rng) + 1;
            lastReason = "random pick weighted toward holes you rarely hit (hole " +
                std::to_string(chosenHole) + " had a " +
                std::to_string(static_cast<int>(probabilities[chosenHole - 1] * 100 + 0.5)) + "% chance)";
            return chosenHole;
        }

        if (explorationRoll(rng)) {
            isExploring = true;
            std::uniform_int_distribution<int> anyHole(1, kHoleCount);
            lastReason = "EXPLORING: picked a random hole to stay unpredictable";
            return anyHole(rng);
        }

        std::vector<int> safestHoles = leastSwungHoles(swingCounts);
        std::uniform_int_distribution<std::size_t> pick(0, safestHoles.size() - 1);
        int chosenHole = safestHoles[pick(rng)];

        if (safestHoles.size() == 1) {
            lastReason = "hole " + std::to_string(chosenHole) + " had the lowest estimate";
        }
        else {
            lastReason = std::to_string(safestHoles.size()) +
                " holes tied for the lowest estimate, so it picked one at random";
        }
        return chosenHole;
    }

    void observeSwing(int hole) {
        ++swingCounts[hole - 1];
        ++roundsObserved;
        swingHistory.push_back(hole);
    }

    void printCalculation(int hole) const {
        std::cout << "  Hole " << hole << ": (" << swingCounts[hole - 1] << " + 1) / ("
            << roundsObserved << " + 2) = " << std::fixed << std::setprecision(3)
            << estimateSwingProbability(hole) << "\n";
    }

    void printEstimateGrid() const {
        std::cout << "  Mole's estimates, P(you swing here next):\n";
        for (int row = 0; row < 3; ++row) {
            std::cout << "   ";
            for (int col = 0; col < 3; ++col) {
                int hole = row * 3 + col + 1;
                std::cout << " " << hole << ":" << std::fixed << std::setprecision(2)
                    << estimateSwingProbability(hole) << " ";
            }
            std::cout << "\n";
        }
    }

    MoleRule getRule() const { return rule; }
    bool wasExploring() const { return isExploring; }
    const std::string& getLastReason() const { return lastReason; }
    const std::vector<int>& getHistory() const { return swingHistory; }
    const SwingCounts& getSwingCounts() const { return swingCounts; }
    int getSwingCount(int hole) const { return swingCounts[hole - 1]; }
    int getRoundsObserved() const { return roundsObserved; }

private:
    MoleRule rule;
    SwingCounts swingCounts{};
    int roundsObserved = 0;
    std::vector<int> swingHistory;
    std::string lastReason;
    bool isExploring = false;
    std::mt19937 rng;
    std::bernoulli_distribution explorationRoll;
};

class AttackBot {
public:
    static HoleProbabilities predictHiding(const MoleAI& mole) {
        return hidingDistribution(mole.getRule(), mole.getSwingCounts(), mole.getRoundsObserved());
    }

    static int bestGuess(const MoleAI& mole, std::mt19937& rng) {
        HoleProbabilities prediction = predictHiding(mole);
        double highest = 0.0;
        for (double probability : prediction) {
            if (probability > highest) highest = probability;
        }
        std::vector<int> bestHoles;
        for (int hole = 1; hole <= kHoleCount; ++hole) {
            if (prediction[hole - 1] > highest - 1e-12) bestHoles.push_back(hole);
        }
        std::uniform_int_distribution<std::size_t> pick(0, bestHoles.size() - 1);
        return bestHoles[pick(rng)];
    }

    static void printHint(const MoleAI& mole) {
        std::cout << "  HINT (AttackBot): where the mole is probably hiding\n";
        printPercentGrid(predictHiding(mole));
    }
};

class SimulatedPlayer {
public:
    SimulatedPlayer(unsigned int seed, int favoriteHole, double trueFavoriteProbability)
        : rng(seed), favorite(favoriteHole), trueProbability(trueFavoriteProbability),
        swingsAtFavorite(trueFavoriteProbability), otherHole(1, kHoleCount - 1) {
    }

    int chooseSwing() {
        if (swingsAtFavorite(rng)) return favorite;
        int hole = otherHole(rng);
        return hole >= favorite ? hole + 1 : hole;
    }

    int getFavoriteHole() const { return favorite; }
    double getTrueProbability() const { return trueProbability; }

private:
    std::mt19937 rng;
    int favorite;
    double trueProbability;
    std::bernoulli_distribution swingsAtFavorite;
    std::uniform_int_distribution<int> otherHole;
};

void printNumberedBoard() {
    std::cout << "   [1] [2] [3]\n   [4] [5] [6]\n   [7] [8] [9]\n";
}

void printResultBoard(int moleHole, int swingHole) {
    for (int row = 0; row < 3; ++row) {
        std::cout << "   ";
        for (int col = 0; col < 3; ++col) {
            int hole = row * 3 + col + 1;
            char mark = '.';
            if (hole == moleHole && hole == swingHole) mark = '*';
            else if (hole == moleHole) mark = 'M';
            else if (hole == swingHole) mark = 'X';
            std::cout << "[" << mark << "] ";
        }
        std::cout << "\n";
    }
}

enum class PlayerInput { Swing, Hint, End };

PlayerInput readSwing(int& hole, bool hintAvailable) {
    std::string input;
    while (true) {
        std::cout << "Swing your hammer (1-9)" << (hintAvailable ? ", or H for a hint" : "") << ": ";
        if (!(std::cin >> input)) return PlayerInput::End;
        if (input.size() == 1 && input[0] >= '1' && input[0] <= '9') {
            hole = input[0] - '0';
            return PlayerInput::Swing;
        }
        if (input.size() == 1 && (input[0] == 'H' || input[0] == 'h')) {
            if (hintAvailable) return PlayerInput::Hint;
            std::cout << "No hint available right now.\n";
            continue;
        }
        std::cout << "Please type a number from 1 to 9" << (hintAvailable ? " or H" : "") << ".\n";
    }
}

bool askYesNo(const std::string& question) {
    std::cout << question << " (y/n): ";
    std::string input;
    if (!(std::cin >> input)) return true;
    return !(input[0] == 'n' || input[0] == 'N');
}

MoleRule askMoleRule() {
    std::cout << "Choose your mole: 1) Classic (easier)  2) Unpredictable (harder): ";
    std::string input;
    if (!(std::cin >> input)) return MoleRule::Classic;
    return input == "2" ? MoleRule::Unpredictable : MoleRule::Classic;
}

void playGame(unsigned int seed) {
    int whacks = 0;
    int hintsLeft = kHintsPerGame;

    std::cout << "\nA sneaky mole lives under this 3x3 field. It remembers every swing\n"
        << "you make and hides where it thinks you are LEAST likely to hit.\n"
        << "Whack it " << kWhacksToWin << " times in " << kMaxRounds << " rounds to win!\n"
        << "You have " << kHintsPerGame << " hints per game: type H to see where the mole is probably hiding.\n";

    MoleAI mole(seed, askMoleRule());

    bool showThinking = askYesNo("Show the mole's thinking?");

    for (int round = 1; round <= kMaxRounds && whacks < kWhacksToWin; ++round) {
        std::cout << "\n--- Round " << round << " of " << kMaxRounds << " ---   Whacks: "
            << whacks << "/" << kWhacksToWin << "   Hints left: " << hintsLeft << "\n";

        int moleHole = mole.chooseHidingHole();
        std::cout << "  Mole: \"" << (round == 1 ? "You'll never catch me!"
            : "I know your habits now...")
            << "\"\n";
        printNumberedBoard();

        int swingHole = 0;
        PlayerInput input = readSwing(swingHole, hintsLeft > 0);
        if (input == PlayerInput::Hint) {
            --hintsLeft;
            AttackBot::printHint(mole);
            input = readSwing(swingHole, false);
        }
        if (input == PlayerInput::End) {
            std::cout << "\nInput ended. Leaving the game.\n";
            break;
        }

        printResultBoard(moleHole, swingHole);
        if (swingHole == moleHole) {
            ++whacks;
            std::cout << "  WHACK! You got the mole in hole " << moleHole << "!\n";
            std::cout << "  Mole: \"Ow! How did you know?!\"\n";
        }
        else {
            std::cout << "  Miss! The mole was in hole " << moleHole << ".\n";
            std::cout << "  Mole: \"Hehe, too slow!\"\n";
        }
        if (showThinking) {
            std::cout << "  Why hole " << moleHole << "? " << mole.getLastReason() << ".\n";
        }

        mole.observeSwing(swingHole);
        if (showThinking) {
            mole.printCalculation(swingHole);
            mole.printEstimateGrid();
        }
    }

    std::cout << "\n=== GAME OVER ===\n";
    if (!showThinking) {
        std::cout << "Here is what the mole was thinking by the end:\n";
        mole.printEstimateGrid();
    }
    if (whacks >= kWhacksToWin) {
        std::cout << "You won! You out-thought the " << ruleName(mole.getRule()) << " mole.\n";
    }
    else {
        std::cout << "The mole wins. You whacked it " << whacks << " time(s).\n";
    }
    std::cout << "Your swing history: ";
    for (int hole : mole.getHistory()) std::cout << hole << " ";
    std::cout << "\n";
}

void runSimulation(unsigned int seed) {
    const int simulationRounds = 100;
    const int favoriteHole = 5;
    const double trueFavoriteProbability = 0.40;

    MoleAI mole(seed, MoleRule::Classic);
    SimulatedPlayer player(seed + 1, favoriteHole, trueFavoriteProbability);
    int whacks = 0;

    std::cout << "\nSimulation: the computer swings at hole " << favoriteHole
        << " with a hidden TRUE probability of " << std::fixed << std::setprecision(2)
        << trueFavoriteProbability << " (other holes at random).\n"
        << "The mole does not know this number. It only sees the swings.\n\n"
        << "Round | Swing | Mole | Result | Mole's estimate for hole " << favoriteHole << "\n"
        << "------+-------+------+--------+----------------------------\n";

    for (int round = 1; round <= simulationRounds; ++round) {
        int moleHole = mole.chooseHidingHole();
        int swingHole = player.chooseSwing();
        bool whacked = (swingHole == moleHole);
        if (whacked) ++whacks;
        mole.observeSwing(swingHole);

        if (round <= 10 || round % 10 == 0) {
            std::cout << std::setw(5) << round << " | " << std::setw(5) << swingHole << " | "
                << std::setw(4) << moleHole << " | " << std::setw(6)
                << (whacked ? "WHACK" : "miss") << " | " << std::setprecision(3)
                << mole.estimateSwingProbability(favoriteHole) << "\n";
        }
    }

    int favoriteSwings = mole.getSwingCount(favoriteHole);
    std::cout << "\n=== SIMULATION RESULTS ===\n" << std::setprecision(3)
        << "TRUE probability of hole " << favoriteHole << " (hidden): "
        << player.getTrueProbability() << "\n"
        << "Observed frequency:                 " << favoriteSwings << " / "
        << simulationRounds << " = "
        << static_cast<double>(favoriteSwings) / simulationRounds << "\n"
        << "Mole's Laplace estimate:            (" << favoriteSwings << " + 1) / ("
        << simulationRounds << " + 2) = " << mole.estimateSwingProbability(favoriteHole)
        << "\n"
        << "Whacks against the mole: " << whacks << " / " << simulationRounds << "\n";
}

enum class AttackStrategy { SameHole, Random, AttackBot };

std::string strategyName(AttackStrategy strategy) {
    switch (strategy) {
    case AttackStrategy::SameHole:  return "Always swing at the same hole";
    case AttackStrategy::Random:    return "Swing at random";
    case AttackStrategy::AttackBot: return "AttackBot (swing where the mole should hide)";
    }
    return "";
}

bool playAttackGame(AttackStrategy strategy, MoleRule rule, std::mt19937& rng) {
    MoleAI mole(rng(), rule);
    std::uniform_int_distribution<int> anyHole(1, kHoleCount);
    int favoriteHole = anyHole(rng);
    int whacks = 0;

    for (int round = 1; round <= kMaxRounds && whacks < kWhacksToWin; ++round) {
        int moleHole = mole.chooseHidingHole();
        int swingHole = 0;
        if (strategy == AttackStrategy::SameHole) swingHole = favoriteHole;
        else if (strategy == AttackStrategy::Random) swingHole = anyHole(rng);
        else swingHole = AttackBot::bestGuess(mole, rng);

        if (swingHole == moleHole) ++whacks;
        mole.observeSwing(swingHole);
    }
    return whacks >= kWhacksToWin;
}

double attackWinRate(AttackStrategy strategy, MoleRule rule, int games, std::mt19937& rng) {
    int wins = 0;
    for (int game = 0; game < games; ++game) {
        if (playAttackGame(strategy, rule, rng)) ++wins;
    }
    return 100.0 * wins / games;
}

void runRedTeam(unsigned int seed) {
    const int gamesPerTest = 10000;
    std::mt19937 rng(seed);

    std::cout << "\nRed-team test: each strategy plays " << gamesPerTest << " full games ("
        << kMaxRounds << " rounds, " << kWhacksToWin << " whacks to win) against each mole.\n\n"
        << "  " << std::left << std::setw(46) << "Attack strategy" << std::right
        << std::setw(10) << "Classic" << std::setw(16) << "Unpredictable" << "\n"
        << "  " << std::string(72, '-') << "\n";

    for (AttackStrategy strategy : { AttackStrategy::SameHole, AttackStrategy::Random, AttackStrategy::AttackBot }) {
        double classicRate = attackWinRate(strategy, MoleRule::Classic, gamesPerTest, rng);
        double unpredictableRate = attackWinRate(strategy, MoleRule::Unpredictable, gamesPerTest, rng);
        std::cout << "  " << std::left << std::setw(46) << strategyName(strategy) << std::right
            << std::fixed << std::setprecision(1)
            << std::setw(9) << classicRate << "%" << std::setw(15) << unpredictableRate << "%\n";
    }
    std::cout << "\nThe Classic mole always hides in a least-swung hole, so an attacker who knows\n"
        << "the rule can predict it. The Unpredictable mole still avoids your habits, but\n"
        << "picks at random with weights, so knowing the rule helps much less.\n";
}

int main() {
    std::random_device randomDevice;
    unsigned int seed = randomDevice();

    std::cout << "=== THE SNEAKY MOLE ===\n"
        << "1) Play the game\n"
        << "2) Run simulation (check the mole's learning)\n"
        << "3) Red-team test (let the AttackBot try to beat the mole)\n"
        << "Choose 1, 2, or 3: ";

    std::string choice;
    if (!(std::cin >> choice)) return 0;
    if (choice == "2") {
        runSimulation(seed);
    }
    else if (choice == "3") {
        runRedTeam(seed);
    }
    else {
        playGame(seed);
    }
    return 0;
}