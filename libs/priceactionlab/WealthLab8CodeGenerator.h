// WealthLab8CodeGenerator.h — full replacement with:
//   - “pyramids = adds” semantics
//   - Max Risk % stop override
//   - Skip-when-both-sides-fire toggle (default: enabled)
// -----------------------------------------------------------------------------
//
// What’s new here:
//  • Adds WL8 parameter: paramSkipIfBothSides (0/1), default 1 (enabled).
//  • In flat state, if both long & short signals fire and the toggle is ON,
//    the strategy stands aside for that bar.
//  • Otherwise proceeds normally, with explicit tie preference preserved.
//
// Previously added (retained):
//  • Generator ctor accepts (double longStopPercent, double shortStopPercent)
//  • Generated C# fields mLongStopPercent, mShortStopPercent
//  • Generated C# override GetMaxRiskStopLevel(...) for Max Risk % sizing
//  • Volume accessor V(int) in EnterLong/EnterShort
//  • StartIndex uses PAL getMaxBarsBack() with explanatory comment
//  • Explicit long/short tie preference in Execute
//  • Clarified per-symbol OpenPositions counting comment
//  • Note on getpatternIndex() lowercase 'p'
//  • Unsupported-node errors include current pattern index
// -----------------------------------------------------------------------------

#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <memory>
#include <vector>
#include <algorithm>
#include <iomanip>

#include "PalAst.h"
#include "PalCodeGenVisitor.h"

class WealthLab8CodeGenVisitor : public PalCodeGenVisitor
{
public:
    // Pass percent stops (e.g., 2.0 == 2%) to embed in generated C# as literals
    WealthLab8CodeGenVisitor(std::shared_ptr<PriceActionLabSystem> system,
                             std::string outputFileName,
                             std::string className,
                             double longStopPercent = 2.0,
                             double shortStopPercent = 2.0)
        : mSystem(std::move(system))
        , mOutputFileName(std::move(outputFileName))
        , mClassName(std::move(className))
        , mCurrentPatternIndex(0)
        , mLongStopPercent(longStopPercent)
        , mShortStopPercent(shortStopPercent)
    {}

    void generateCode()
    {
        mOut.open(mOutputFileName.c_str(), std::ios::out | std::ios::trunc);
        if (!mOut.is_open())
            throw std::runtime_error("Cannot open output file: " + mOutputFileName);

        // Pre-scan StartIndex via PAL pattern metadata (already accounts for lookback).
        int maxBarsBack = 0;
        for (auto it = mSystem->patternLongsBegin(); it != mSystem->patternLongsEnd(); ++it)
            maxBarsBack = std::max(maxBarsBack, static_cast<int>(it->second->getMaxBarsBack()));
        for (auto it = mSystem->patternShortsBegin(); it != mSystem->patternShortsEnd(); ++it)
            maxBarsBack = std::max(maxBarsBack, static_cast<int>(it->second->getMaxBarsBack()));

        // NOTE: getMaxBarsBack() comes from PalPatternMaxBars::evaluateExpression and already
        // includes extra-bars-needed semantics. No +1 bump required.
        const int startIndex = maxBarsBack;

        writeFilePreamble();
        writeClassPreamble();
        writeConstructor();
        writeInitialize(startIndex);
        writeExecute();             // explicit tie preference + "adds" semantics + both-sides toggle
        writeEnterLong();           // includes V(int)
        writeEnterShort();          // includes V(int)
        writeGetMaxRiskStopLevel(); // WL8 Max Risk % sizing support
        writePrivateMembers();
        writePosInfo();
        writeNamespaceEpilogue();

        mOut.close();
    }

    // -------------------- Visitor overrides for expression building --------------------
    void visit(PriceBarOpen* n) override        { emitRef("O", n->getBarOffset()); }
    void visit(PriceBarHigh* n) override        { emitRef("H", n->getBarOffset()); }
    void visit(PriceBarLow* n) override         { emitRef("L", n->getBarOffset()); }
    void visit(PriceBarClose* n) override       { emitRef("C", n->getBarOffset()); }
    void visit(VolumeBarReference* n) override  { emitRef("V", n->getBarOffset()); } // volume support

    void visit(Roc1BarReference*) override        { unsupportedNode("ROC1"); }
    void visit(MeanderBarReference*) override     { unsupportedNode("Meander"); }
    void visit(VChartLowBarReference*) override   { unsupportedNode("VChartLow"); }
    void visit(VChartHighBarReference*) override  { unsupportedNode("VChartHigh"); }
    void visit(IBS1BarReference*) override        { unsupportedNode("IBS1"); }
    void visit(IBS2BarReference*) override        { unsupportedNode("IBS2"); }
    void visit(IBS3BarReference*) override        { unsupportedNode("IBS3"); }

    void visit(GreaterThanExpr* n) override
    {
        mExpr << "(";
        n->getLHS()->accept(*this);
        mExpr << " > ";
        n->getRHS()->accept(*this);
        mExpr << ")";
    }

    void visit(AndExpr* n) override
    {
        n->getLHS()->accept(*this);
        mExpr << " && ";
        n->getRHS()->accept(*this);
    }

    void visit(LongMarketEntryOnOpen*) override {}
    void visit(ShortMarketEntryOnOpen*) override {}
    void visit(PatternDescription*) override {}
    void visit(PriceActionLabPattern*) override {}
    void visit(LongSideProfitTargetInPercent*) override {}
    void visit(ShortSideProfitTargetInPercent*) override {}
    void visit(LongSideStopLossInPercent*) override {}
    void visit(ShortSideStopLossInPercent*) override {}

private:
    // ------------------------------- Emission helpers ---------------------------------
    [[noreturn]] void unsupportedNode(const char* name)
    {
        // Include current pattern index to speed triage.
        std::ostringstream oss;
        oss << "WealthLab8CodeGenVisitor: unsupported AST node: " << name
            << " (patternIndex=" << mCurrentPatternIndex << ")";
        throw std::domain_error(oss.str());
    }

    void writeFilePreamble()
    {
        mOut << "using WealthLab.Backtest;\n";
        mOut << "using System;\n";
        mOut << "using WealthLab.Core;\n";
        mOut << "using WealthLab.Data;\n";
        mOut << "using WealthLab.Indicators;\n";
        mOut << "using System.Collections.Generic;\n";
        mOut << "namespace WealthScript1\n";
        mOut << "{\n";
    }

    void writeNamespaceEpilogue()
    {
        mOut << "}\n"; // namespace WealthScript1
    }

    void writeClassPreamble()
    {
        mOut << "    public class " << mClassName << " : UserStrategyBase\n";
        mOut << "    {\n";
        // Emit fields for Max Risk % stops (in percent units, e.g., 2.0 == 2%) as literals
        mOut << "        // Max Risk % stop configuration (percent units)\n";
        mOut << "        private double mLongStopPercent = " << std::fixed << std::setprecision(8) << mLongStopPercent << ";\n";
        mOut << "        private double mShortStopPercent = " << std::fixed << std::setprecision(8) << mShortStopPercent << ";\n";
    }

    void writeConstructor()
    {
        mOut << "        //constructor\n";
        mOut << "        public " << mClassName << "()\n";
        mOut << "        {\n";
        mOut << "            paramEnablePyramiding = AddParameter(\"Enable Pyramiding\", ParameterType.Int32, 0, 0, 1, 1);\n";
        // Label clarifies “adds” (not total positions)
        mOut << "            paramMaxPyramids     = AddParameter(\"Max Pyramids (adds)\", ParameterType.Int32, 3, 0, 10, 1);\n";
        mOut << "            paramMaxHold        = AddParameter(\"Max Hold Period\", ParameterType.Int32, 8, 5, 50, 5);\n";
        // NEW: 0/1 toggle; default 1 (stand aside when both sides fire from flat)
        mOut << "            paramSkipIfBothSides = AddParameter(\"Skip if Long & Short fire (flat)\", ParameterType.Int32, 1, 0, 1, 1);\n";
        mOut << "            // NOTE: mLongStopPercent / mShortStopPercent are embedded literals from the generator.\n";
        mOut << "            // If you prefer WL8 UI parameters for these, expose them via AddParameter and assign here.\n";
        mOut << "        }\n";
    }

    void writeInitialize(int startIndex)
    {
        mOut << "        //create indicators and other objects here, this is executed prior to the main trading loop\n";
        mOut << "        public override void Initialize(BarHistory bars)\n";
        mOut << "        {\n";
        mOut << "            // StartIndex derived from PAL getMaxBarsBack() (already includes lookback needs).\n";
        mOut << "            StartIndex = " << startIndex << ";\n";
        mOut << "        }\n";
    }

    void writeExecute()
    {
        // Emit Execute with explicit tie preference (true = Long-first, false = Short-first).
        constexpr bool preferLongOnTies = true;

        mOut << "        //execute the strategy rules here, this is executed once for each bar in the backtest history\n";
        mOut << "        public override void Execute(BarHistory bars, int idx)\n";
        mOut << "        {\n";
        mOut << "            //determine if we should check to go long or short\n";
        mOut << "            bool allowPyramid = (paramEnablePyramiding.AsInt == 0) ? false : true;\n";
        mOut << "            int maxAdds = paramMaxPyramids.AsInt;\n";
        mOut << "            int allowedTotal = 1 + maxAdds; // initial + additional pyramids\n";
        mOut << "            int openLong = 0, openShort = 0;\n";
        mOut << "            foreach (Position p in OpenPositions)\n";
        mOut << "            {\n";
        mOut << "                if (!ReferenceEquals(p.Bars, bars)) continue; // per-symbol count only\n";
        mOut << "                if (p.PositionType == PositionType.Long) openLong++;\n";
        mOut << "                else if (p.PositionType == PositionType.Short) openShort++;\n";
        mOut << "            }\n";
        mOut << "            bool hasAny = (openLong + openShort) > 0;\n";
        mOut << "            bool goLong = false, goShort = false;\n";
        mOut << "            if (!allowPyramid)\n";
        mOut << "            {\n";
        mOut << "                goLong = !hasAny;\n";
        mOut << "                goShort = !hasAny;\n";
        mOut << "            }\n";
        mOut << "            else\n";
        mOut << "            {\n";
        mOut << "                if (openLong > 0 && openShort == 0)\n";
        mOut << "                {\n";
        mOut << "                    goLong = openLong < allowedTotal;\n";
        mOut << "                    goShort = false;\n";
        mOut << "                }\n";
        mOut << "                else if (openShort > 0 && openLong == 0)\n";
        mOut << "                {\n";
        mOut << "                    goShort = openShort < allowedTotal;\n";
        mOut << "                    goLong = false;\n";
        mOut << "                }\n";
        mOut << "                else if (!hasAny)\n";
        mOut << "                {\n";
        mOut << "                    goLong = true;\n";
        mOut << "                    goShort = true;\n";
        mOut << "                }\n";
        mOut << "                else\n";
        mOut << "                {\n";
        mOut << "                    goLong = false;\n";
        mOut << "                    goShort = false;\n";
        mOut << "                }\n";
        mOut << "            }\n";

        // NEW: If flat and both directions are allowed, optionally stand aside when both fire
        mOut << "            if (!hasAny && goLong && goShort && paramSkipIfBothSides.AsInt != 0)\n";
        mOut << "            {\n";
        mOut << "                bool longSignal = EnterLong(bars, idx);\n";
        mOut << "                bool shortSignal = EnterShort(bars, idx);\n";
        mOut << "                if (longSignal && shortSignal)\n";
        mOut << "                    return; // stand aside this bar\n";
        mOut << "                // otherwise, proceed only with the side that actually signaled\n";
        mOut << "                goLong = longSignal;\n";
        mOut << "                goShort = shortSignal;\n";
        mOut << "            }\n";

        if (preferLongOnTies)
        {
            emitEnterSideBlock(/*isLong*/true);
            emitEnterSideBlock(/*isLong*/false);
        }
        else
        {
            emitEnterSideBlock(/*isLong*/false);
            emitEnterSideBlock(/*isLong*/true);
        }

        mOut << "            //issue exits for existing positions\n";
        mOut << "            foreach (Position position in OpenPositions)\n";
        mOut << "            {\n";
        mOut << "                if (idx - position.EntryBar >= paramMaxHold.AsInt)\n";
        mOut << "                    ClosePosition(position, OrderType.Market, 0, \"Max Hold\");\n";
        mOut << "                else\n";
        mOut << "                {\n";
        mOut << "                    PosInfo pi = (PosInfo)position.Tag;\n";
        mOut << "                    ClosePosition(position, OrderType.Stop, position.EntryPrice * pi.StopLoss, \"Stop\");\n";
        mOut << "                    ClosePosition(position, OrderType.Limit, position.EntryPrice * pi.ProfitTarget, \"Profit\");\n";
        mOut << "                }\n";
        mOut << "            }\n";
        mOut << "        }\n";
    }

    // Helper to emit goLong/goShort guarded entry blocks
    void emitEnterSideBlock(bool isLong)
    {
        const char* guard = isLong ? "goLong" : "goShort";
        const char* entryMethod = isLong ? "EnterLong" : "EnterShort";
        const char* txType = isLong ? "TransactionType.Buy" : "TransactionType.Short";
        const char* killOther = isLong ? "goShort" : "goLong";

        mOut << "            if (" << guard << ")\n";
        mOut << "            {\n";
        mOut << "                if (" << entryMethod << "(bars, idx))\n";
        mOut << "                {\n";
        mOut << "                    Transaction t = PlaceTrade(bars, " << txType << ", OrderType.Market, 0, patternNumber.ToString());\n";
        mOut << "                    PosInfo pi = new PosInfo();\n";
        mOut << "                    pi.StopLoss = stop;\n";
        mOut << "                    pi.ProfitTarget = profit;\n";
        mOut << "                    t.Tag = pi;\n";
        mOut << "                    " << killOther << " = false; // enforce one-side-per-bar explicitly\n";
        mOut << "                }\n";
        mOut << "            }\n";
    }

    void writeEnterLong()
    {
        mOut << "        // LONG patterns evaluated inline (no interpreter)\n";
        mOut << "        public bool EnterLong(BarHistory bars, int idx)\n";
        mOut << "        {\n";
        mOut << "            if (idx < StartIndex) return false; // safety\n";
        mOut << "            // shorthand accessors scoped to this method\n";
        mOut << "            double O(int n) => bars.Open[idx - n];\n";
        mOut << "            double H(int n) => bars.High[idx - n];\n";
        mOut << "            double L(int n) => bars.Low[idx - n];\n";
        mOut << "            double C(int n) => bars.Close[idx - n];\n";
        mOut << "            double V(int n) => bars.Volume[idx - n]; // Volume accessor for patterns\n";

        for (auto it = mSystem->patternLongsBegin(); it != mSystem->patternLongsEnd(); ++it)
        {
            emitPatternBlock(it->second.get(), /*isLong*/ true);
        }

        mOut << "            return false;\n";
        mOut << "        }\n";
    }

    void writeEnterShort()
    {
        mOut << "        // SHORT patterns evaluated inline (no interpreter)\n";
        mOut << "        public bool EnterShort(BarHistory bars, int idx)\n";
        mOut << "        {\n";
        mOut << "            if (idx < StartIndex) return false; // safety\n";
        mOut << "            // shorthand accessors scoped to this method\n";
        mOut << "            double O(int n) => bars.Open[idx - n];\n";
        mOut << "            double H(int n) => bars.High[idx - n];\n";
        mOut << "            double L(int n) => bars.Low[idx - n];\n";
        mOut << "            double C(int n) => bars.Close[idx - n];\n";
        mOut << "            double V(int n) => bars.Volume[idx - n]; // Volume accessor for patterns\n";

        for (auto it = mSystem->patternShortsBegin(); it != mSystem->patternShortsEnd(); ++it)
        {
            emitPatternBlock(it->second.get(), /*isLong*/ false);
        }

        mOut << "            return false;\n";
        mOut << "        }\n";
    }

    // Provide a customized initial Stop Loss level for Max Risk % sizing
    void writeGetMaxRiskStopLevel()
    {
        mOut << "        // Provide a customized initial Stop Loss level for Max Risk % sizing\n";
        mOut << "        // WL8 will call this to compute position size based on your configured percent stops.\n";
        mOut << "        public override double GetMaxRiskStopLevel(BarHistory bars, PositionType pt, int idx)\n";
        mOut << "        {\n";
        mOut << "            // Compute from the close using side-specific percent\n";
        mOut << "            double referencePrice = bars.Close[idx];\n";
        mOut << "            double referenceStopPercent = (pt == PositionType.Long) ? mLongStopPercent : mShortStopPercent;\n";
        mOut << "            double frac = referenceStopPercent / 100.0; // convert percent → fraction\n";
        mOut << "            double offset = referencePrice * frac;\n";
        mOut << "            double stop = (pt == PositionType.Long) ? (referencePrice - offset) : (referencePrice + offset);\n";
        mOut << "            return stop;\n";
        mOut << "        }\n";
    }

    void writePrivateMembers()
    {
        mOut << "        //declare private variables below\n";
        mOut << "        private Parameter paramEnablePyramiding;\n";
        mOut << "        private Parameter paramMaxPyramids;\n";
        mOut << "        private Parameter paramMaxHold;\n";
        mOut << "        private Parameter paramSkipIfBothSides;\n";
        mOut << "        private double stop;\n";
        mOut << "        private double profit;\n";
        mOut << "        private int patternNumber;\n";
    }

    void writePosInfo()
    {
        mOut << "    }\n"; // close strategy class
        mOut << "    public class PosInfo\n";
        mOut << "    {\n";
        mOut << "        public double ProfitTarget { get; set; }\n";
        mOut << "        public double StopLoss { get; set; }\n";
        mOut << "    }\n";
    }

    // --------------------------- Pattern emission & helpers ---------------------------
    void emitPatternBlock(PriceActionLabPattern* pat, bool isLong)
    {
        // Track pattern index for richer error messages.
        const auto* desc = pat->getPatternDescription().get();
        // NOTE: method name is getpatternIndex() with a lowercase 'p' in PAL.
        const int patIdx = desc ? desc->getpatternIndex() : 0;
        const int savedIdx = mCurrentPatternIndex;
        mCurrentPatternIndex = patIdx;

        std::vector<std::string> clauses;
        collectConjuncts(pat->getPatternExpression().get(), clauses);
        if (clauses.empty())
        {
            std::string expr = buildAtomicString(pat->getPatternExpression().get());
            if (!expr.empty()) clauses.push_back(expr);
        }

        mOut << "            // pattern " << patIdx << "\n";
        mOut << "            if\n";
        mOut << "            (\n";
        for (size_t i = 0; i < clauses.size(); ++i)
        {
            mOut << "                " << clauses[i];
            if (i + 1 < clauses.size()) mOut << " &&";
            mOut << "\n";
        }
        mOut << "            )\n";
        mOut << "            {\n";

        const double stopPct = pat->getStopLossAsDecimal().getAsDouble();
        const double prftPct = pat->getProfitTargetAsDecimal().getAsDouble();
        mOut << std::fixed << std::setprecision(8);
        if (isLong)
        {
            mOut << "                stop = 1.0 - (" << stopPct << " / 100.0);\n";
            mOut << "                profit = 1.0 + (" << prftPct << " / 100.0);\n";
        }
        else
        {
            mOut << "                stop = 1.0 + (" << stopPct << " / 100.0);\n";
            mOut << "                profit = 1.0 - (" << prftPct << " / 100.0);\n";
        }
        mOut << "                patternNumber = " << patIdx << ";\n";
        mOut << "                return true;\n";
        mOut << "            }\n";

        mCurrentPatternIndex = savedIdx;
    }

    void collectConjuncts(PatternExpression* node, std::vector<std::string>& out)
    {
        if (!node) return;

        if (auto* andNode = dynamic_cast<AndExpr*>(node))
        {
            collectConjuncts(andNode->getLHS(), out);
            collectConjuncts(andNode->getRHS(), out);
            return;
        }

        std::string atom = buildAtomicString(node);
        if (!atom.empty())
            out.push_back(atom);
    }

    std::string buildAtomicString(PatternExpression* node)
    {
        if (!node) return {};
        mExpr.str(std::string());
        mExpr.clear();
        node->accept(*this);
        return mExpr.str();
    }

    void emitRef(const char* fn, unsigned int offset)
    {
        mExpr << fn << "(" << offset << ")";
    }

private:
    // Inputs and output
    std::shared_ptr<PriceActionLabSystem> mSystem;
    std::string mOutputFileName;
    std::string mClassName;
    std::ofstream mOut;

    // Expr builder
    std::ostringstream mExpr;

    // Context: current pattern id for better diagnostics
    int mCurrentPatternIndex;

    // User-provided percent stops (e.g., 2.0 == 2%)
    double mLongStopPercent;
    double mShortStopPercent;
};
