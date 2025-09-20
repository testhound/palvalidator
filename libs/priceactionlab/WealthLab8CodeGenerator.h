// WealthLab8CodeGenVisitor.h (template-aligned, header-only)
// -----------------------------------------------------------------------------
// Emits a WealthLab 8 C# strategy that follows the provided hand-written
// template exactly (namespace, constructor, parameter handling, Initialize
// signature, Execute/PosInfo usage, pyramiding behavior, etc.).
//
// Key traits matching the template:
//   • namespace WealthScript1
//   • public override void Initialize(BarHistory bars)
//   • Constructor adds parameters via AddParameter (Int32 only), with
//     defaults: Enable Pyramiding = 0, Max Pyramids = 3, Max Hold = 8
//   • Execute() logic enforces one-side-per-bar and same-direction pyramiding
//   • PosInfo is used as in the template; entry tags carry per-trade targets
//   • EnterLong/EnterShort set `stop`, `profit`, and `patternNumber`
//   • Pattern code is emitted in C#/C++ Allman style (braces on new lines, each
//     statement on its own line; multi-clause conditions formatted line-by-line)
//
// AST coverage:
//   - Supported nodes: O/H/L/C/Volume references; GreaterThan; And; MarketEntryOnOpen
//   - Unsupported (throws): ROC1, IBS1/2/3, Meander, VChartLow/High (easy to add)
//
// Usage:
//   auto sys = std::make_shared<PriceActionLabSystem>();
//   // ...populate sys...
//   WealthLab8CodeGenVisitor gen(sys, "GeneratedStrategy.cs", "MyStrategy");
//   gen.generateCode();
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
    WealthLab8CodeGenVisitor(std::shared_ptr<PriceActionLabSystem> system,
                             std::string outputFileName,
                             std::string className)
        : mSystem(std::move(system))
        , mOutputFileName(std::move(outputFileName))
        , mClassName(std::move(className))
    {}

    void generateCode()
    {
        mOut.open(mOutputFileName.c_str(), std::ios::out | std::ios::trunc);
        if (!mOut.is_open())
            throw std::runtime_error("Cannot open output file: " + mOutputFileName);

        // Pre-scan to compute max lookback (StartIndex) using getMaxBarsBack()
        int maxBarsBack = 0;
        for (auto it = mSystem->patternLongsBegin(); it != mSystem->patternLongsEnd(); ++it)
            maxBarsBack = std::max(maxBarsBack, static_cast<int>(it->second->getMaxBarsBack()));
        for (auto it = mSystem->patternShortsBegin(); it != mSystem->patternShortsEnd(); ++it)
            maxBarsBack = std::max(maxBarsBack, static_cast<int>(it->second->getMaxBarsBack()));
        const int startIndex = maxBarsBack;

        writeFilePreamble();
        writeClassPreamble();
        writeConstructor();
        writeInitialize(startIndex);
        writeExecute();
        writeEnterLong();
        writeEnterShort();
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
    void visit(VolumeBarReference* n) override  { emitRef("V", n->getBarOffset()); }

    void visit(Roc1BarReference* bar) override    { unsupportedNode("ROC1"); }
    void visit(MeanderBarReference* bar) override { unsupportedNode("Meander"); }
    void visit(VChartLowBarReference* bar) override  { unsupportedNode("VChartLow"); }
    void visit(VChartHighBarReference* bar) override { unsupportedNode("VChartHigh"); }
    void visit(IBS1BarReference* bar) override    { unsupportedNode("IBS1"); }
    void visit(IBS2BarReference* bar) override    { unsupportedNode("IBS2"); }
    void visit(IBS3BarReference* bar) override    { unsupportedNode("IBS3"); }

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
        // We do not emit directly here; we flatten with collectConjuncts().
        // This override is unused in buildAtomicString(); kept for completeness.
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
    [[noreturn]] void unsupportedNode(const char* name)
    {
        throw std::domain_error(std::string("WealthLab8CodeGenVisitor: unsupported AST node: ") + name);
    }

    // ------------------------------- Emission helpers ---------------------------------
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
        mOut << "	public class " << mClassName << " : UserStrategyBase\n";
        mOut << "	{\n";
    }

    void writeConstructor()
    {
        mOut << "		//constructor\n";
        mOut << "		public " << mClassName << "()\n";
        mOut << "		{\n";
        mOut << "			paramEnablePyramiding = AddParameter(\"Enable Pyramiding\", ParameterType.Int32, 0, 0, 1, 1);\n";
        mOut << "			paramMaxPyramids = AddParameter(\"Max Pyramids\", ParameterType.Int32, 3, 1, 10, 1);\n";
        mOut << "			paramMaxHold = AddParameter(\"Max Hold Period\", ParameterType.Int32, 8, 5, 50, 5);\n";
        mOut << "		}\n";
    }

    void writeInitialize(int startIndex)
    {
        mOut << "		//create indicators and other objects here, this is executed prior to the main trading loop\n";
        mOut << "		public override void Initialize(BarHistory bars)\n";
        mOut << "		{\n";
        mOut << "			StartIndex = " << startIndex << "; // largest lookback used by inline patterns\n";
        mOut << "		}\n";
    }

    void writeExecute()
    {
        // This block mirrors the template's logic verbatim (one-side-per-bar, same-direction pyramiding)
        mOut << "		//execute the strategy rules here, this is executed once for each bar in the backtest history\n";
        mOut << "		public override void Execute(BarHistory bars, int idx)\n";
        mOut << "		{\n";
        mOut << "			//determine if we should check to go long or short\n";
        mOut << "			bool allowPyramid = (paramEnablePyramiding.AsInt == 0) ? false : true;\n";
        mOut << "			int maxPyr = paramMaxPyramids.AsInt;\n";
        mOut << "			int openLong = 0, openShort = 0;\n";
        mOut << "			foreach (Position p in OpenPositions)\n";
        mOut << "			{\n";
        mOut << "				if (!ReferenceEquals(p.Bars, bars)) continue; // per-symbol count\n";
        mOut << "				if (p.PositionType == PositionType.Long) openLong++;\n";
        mOut << "				else if (p.PositionType == PositionType.Short) openShort++;\n";
        mOut << "			}\n";
        mOut << "			bool hasAny = (openLong + openShort) > 0;\n";
        mOut << "			bool goLong = false, goShort = false;\n";
        mOut << "			if (!allowPyramid)\n";
        mOut << "			{\n";
        mOut << "				// No pyramiding: only enter if there are no open positions in any direction\n";
        mOut << "				goLong = !hasAny;\n";
        mOut << "				goShort = !hasAny;\n";
        mOut << "			}\n";
        mOut << "			else\n";
        mOut << "			{\n";
        mOut << "				// Pyramiding only in the SAME direction as existing positions\n";
        mOut << "				if (openLong > 0 && openShort == 0)\n";
        mOut << "				{\n";
        mOut << "					goLong = openLong < maxPyr;\n";
        mOut << "					goShort = false;\n";
        mOut << "				}\n";
        mOut << "				else if (openShort > 0 && openLong == 0)\n";
        mOut << "				{\n";
        mOut << "					goShort = openShort < maxPyr;\n";
        mOut << "					goLong = false;\n";
        mOut << "				}\n";
        mOut << "				else if (!hasAny)\n";
        mOut << "				{\n";
        mOut << "					// No open positions: both directions are allowed to qualify\n";
        mOut << "					goLong = true;\n";
        mOut << "					goShort = true;\n";
        mOut << "				}\n";
        mOut << "				else\n";
        mOut << "				{\n";
        mOut << "					// Mixed long/short already open (shouldn't happen with the one-side-per-bar rule). Disallow new entries.\n";
        mOut << "					goLong = false;\n";
        mOut << "					goShort = false;\n";
        mOut << "				}\n";
        mOut << "			}\n";

        mOut << "			if (goLong)\n";
        mOut << "			{\n";
        mOut << "				if (EnterLong(bars, idx))\n";
        mOut << "				{\n";
        mOut << "					Transaction t = PlaceTrade(bars, TransactionType.Buy, OrderType.Market, 0, patternNumber.ToString());\n";
        mOut << "					PosInfo pi = new PosInfo();\n";
        mOut << "					pi.StopLoss = stop;\n";
        mOut << "					pi.ProfitTarget = profit;\n";
        mOut << "					t.Tag = pi;\n";
        mOut << "					goShort = false;\n";
        mOut << "				}\n";
        mOut << "			}\n";

        mOut << "			if (goShort)\n";
        mOut << "			{\n";
        mOut << "				if (EnterShort(bars, idx))\n";
        mOut << "				{\n";
        mOut << "					Transaction t = PlaceTrade(bars, TransactionType.Short, OrderType.Market, 0, patternNumber.ToString());\n";
        mOut << "					PosInfo pi = new PosInfo();\n";
        mOut << "					pi.StopLoss = stop;\n";
        mOut << "					pi.ProfitTarget = profit;\n";
        mOut << "					t.Tag = pi;\n";
        mOut << "					goLong = false; // enforce one-side-per-bar explicitly\n";
        mOut << "				}\n";
        mOut << "			}\n";

        mOut << "			//issue exits for existing positions\n";
        mOut << "			foreach (Position position in OpenPositions)\n";
        mOut << "			{\n";
        mOut << "				if (idx - position.EntryBar >= paramMaxHold.AsInt)\n";
        mOut << "					ClosePosition(position, OrderType.Market, 0, \"Max Hold\");\n";
        mOut << "				else\n";
        mOut << "				{\n";
        mOut << "					PosInfo pi = (PosInfo)position.Tag;\n";
        mOut << "					ClosePosition(position, OrderType.Stop, position.EntryPrice * pi.StopLoss, \"Stop\");\n";
        mOut << "					ClosePosition(position, OrderType.Limit, position.EntryPrice * pi.ProfitTarget, \"Profit\");\n";
        mOut << "				}\n";
        mOut << "			}\n";
        mOut << "		}\n";
    }

    void writeEnterLong()
    {
        mOut << "		// LONG patterns evaluated inline (no interpreter)\n";
        mOut << "		public bool EnterLong(BarHistory bars, int idx)\n";
        mOut << "		{\n";
        mOut << "			if (idx < StartIndex) return false; // safety\n";
        mOut << "			// shorthand accessors scoped to this method\n";
        mOut << "			double O(int n) => bars.Open[idx - n];\n";
        mOut << "			double H(int n) => bars.High[idx - n];\n";
        mOut << "			double L(int n) => bars.Low[idx - n];\n";
        mOut << "			double C(int n) => bars.Close[idx - n];\n";

        for (auto it = mSystem->patternLongsBegin(); it != mSystem->patternLongsEnd(); ++it)
        {
            emitPatternBlock(it->second.get(), /*isLong*/ true);
        }

        mOut << "			return false;\n";
        mOut << "		}\n";
    }

    void writeEnterShort()
    {
        mOut << "		// SHORT patterns evaluated inline (no interpreter)\n";
        mOut << "		public bool EnterShort(BarHistory bars, int idx)\n";
        mOut << "		{\n";
        mOut << "			if (idx < StartIndex) return false; // safety\n";
        mOut << "			// shorthand accessors scoped to this method\n";
        mOut << "			double O(int n) => bars.Open[idx - n];\n";
        mOut << "			double H(int n) => bars.High[idx - n];\n";
        mOut << "			double L(int n) => bars.Low[idx - n];\n";
        mOut << "			double C(int n) => bars.Close[idx - n];\n";

        for (auto it = mSystem->patternShortsBegin(); it != mSystem->patternShortsEnd(); ++it)
        {
            emitPatternBlock(it->second.get(), /*isLong*/ false);
        }

        mOut << "			return false;\n";
        mOut << "		}\n";
    }

    void writePrivateMembers()
    {
        mOut << "		//declare private variables below\n";
        mOut << "		private Parameter paramEnablePyramiding;\n";
        mOut << "		private Parameter paramMaxPyramids;\n";
        mOut << "		private Parameter paramMaxHold;\n";
        mOut << "		private double stop;\n";
        mOut << "		private double profit;\n";
        mOut << "		private int patternNumber;\n";
    }

    void writePosInfo()
    {
        mOut << "	}\n"; // close class
        mOut << "	public class PosInfo\n";
        mOut << "	{\n";
        mOut << "		public double ProfitTarget { get; set; }\n";
        mOut << "		public double StopLoss { get; set; }\n";
        mOut << "	}\n";
    }

    // --------------------------- Pattern emission & helpers ---------------------------
    void emitPatternBlock(PriceActionLabPattern* pat, bool isLong)
    {
        std::vector<std::string> clauses;
        collectConjuncts(pat->getPatternExpression().get(), clauses);
        if (clauses.empty())
        {
            std::string expr = buildAtomicString(pat->getPatternExpression().get());
            clauses.push_back(expr);
        }

        auto* desc = pat->getPatternDescription().get();
        int patIdx = desc ? desc->getpatternIndex() : 0;

        mOut << "			// pattern " << patIdx << "\n";
        mOut << "			if\n";
        mOut << "			(\n";
        for (size_t i = 0; i < clauses.size(); ++i)
        {
            mOut << "				" << clauses[i];
            if (i + 1 < clauses.size()) mOut << " &&";
            mOut << "\n";
        }
        mOut << "			)\n";
        mOut << "			{\n";

        const double stopPct = pat->getStopLossAsDecimal().getAsDouble();
        const double prftPct = pat->getProfitTargetAsDecimal().getAsDouble();
        mOut << std::fixed << std::setprecision(8);
        if (isLong)
        {
            mOut << "				stop = 1.0 - (" << stopPct << " / 100.0);\n";
            mOut << "				profit = 1.0 + (" << prftPct << " / 100.0);\n";
        }
        else
        {
            mOut << "				stop = 1.0 + (" << stopPct << " / 100.0);\n";
            mOut << "				profit = 1.0 - (" << prftPct << " / 100.0);\n";
        }
        mOut << "				patternNumber = " << patIdx << ";\n";
        mOut << "				return true;\n";
        mOut << "			}\n";
    }

    void collectConjuncts(PatternExpression* node, std::vector<std::string>& out)
    {
        if (!node) return;
        if (auto* a = dynamic_cast<AndExpr*>(node))
        {
            collectConjuncts(a->getLHS(), out);
            collectConjuncts(a->getRHS(), out);
            return;
        }
        out.push_back(buildAtomicString(node));
    }

    std::string buildAtomicString(PatternExpression* node)
    {
        mExpr.str("");
        mExpr.clear();
        node->accept(*this);
        return mExpr.str();
    }

    void scanMaxOffset(PatternExpression* node)
    {
        if (!node) return;
        auto prev = mTrackOffsetsOnly;
        mTrackOffsetsOnly = true;
        (void)buildAtomicString(node);
        mTrackOffsetsOnly = prev;
    }

    void emitRef(const char* prefix, int offset)
    {
        if (mTrackOffsetsOnly)
            mGlobalMaxOffset = std::max(mGlobalMaxOffset, offset);
        mExpr << "(" << prefix << "(" << offset << ")" << ")"; // parenthesize atomic refs
    }

private:
    std::shared_ptr<PriceActionLabSystem> mSystem;
    std::string mOutputFileName;
    std::string mClassName;
    std::ofstream mOut;

    std::ostringstream mExpr;
    bool mTrackOffsetsOnly{false};
    int mGlobalMaxOffset{0};
};
