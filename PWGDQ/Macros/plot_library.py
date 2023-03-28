import array as arr
import numpy as np
import os
import sys
import argparse
import ROOT
from os import path
from ROOT import TCanvas, TLatex, TF1, TFile, TPaveText, TMath, TH1F, TString, TLegend, TRatioPlot, TGaxis
from ROOT import gROOT, gBenchmark, gPad, gStyle, kTRUE, kFALSE

def SetLatex(latex):
    latex.SetTextSize(0.035)
    latex.SetNDC()
    latex.SetTextFont(42)

def SetLegend(legend):
    legend.SetBorderSize(0)
    legend.SetFillColor(10)
    legend.SetFillStyle(1)
    legend.SetLineStyle(0)
    legend.SetLineColor(0)
    legend.SetTextFont(42)
    legend.SetTextSize(0.03)

def LoadStyle():
    gStyle.SetPadLeftMargin(0.15)
    gStyle.SetPadBottomMargin(0.15)
    gStyle.SetPadTopMargin(0.05)
    gStyle.SetPadRightMargin(0.05)
    gStyle.SetEndErrorSize(0.0)
    gStyle.SetTitleSize(0.05,"X")
    gStyle.SetTitleSize(0.045,"Y")
    gStyle.SetLabelSize(0.045,"X")
    gStyle.SetLabelSize(0.045,"Y")
    gStyle.SetTitleOffset(1.2,"X")
    gStyle.SetTitleOffset(1.35,"Y")

def DoRatioPlot(pdf, data, hist, var, trialName):
    LoadStyle()
    pt_modelobs = pdf.getObservables(data)
    pt_modelPars = pdf.getParameters(pt_modelobs)
    tf1FromPdf = pdf.asTF(pt_modelobs, pt_modelPars, var)
    canvasRatio = TCanvas("ratio_plot_{}".format(trialName), "ratio_plot_{}".format(trialName))
    #histClone = hist.Clone("histClone")
    hist.Scale(1. / hist.Integral(), "width")
    hist.Divide(tf1FromPdf)
    hist.Draw("E0")
    #tf1FromPdf.Draw()
    #ratioPlot = TRatioPlot(hist1, hist2)
    #ratioPlot.Draw()
    #ratioPlot.GetLowerRefYaxis().SetRangeUser(0.,2.)
    #ratioPlot.GetLowerRefYaxis().SetLabelSize(0.025)
    canvasRatio.Update()
    canvasRatio.Draw()
    return canvasRatio

def DoResidualPlot(rooPlot, rooVar, trialName):
    rooHistResidual = rooPlot.residHist()
    rooPlotResidual = rooVar.frame(ROOT.RooFit.Title("Residual Distribution"))
    rooPlotResidual.addPlotable(rooHistResidual,"P")
    canvasResidual = TCanvas("residual_plot_{}".format(trialName), "residual_plot_{}".format(trialName), 600, 600)
    canvasResidual.SetLeftMargin(0.15)
    rooPlotResidual.GetYaxis().SetTitleOffset(1.4)
    rooPlotResidual.Draw()
    return canvasResidual

def DoPullPlot(rooPlot, rooVar, trialName):
    rooHistPull = rooPlot.pullHist()
    rooPlotPull = rooVar.frame(ROOT.RooFit.Title("Pull Distribution"))
    rooPlotPull.addPlotable(rooHistPull,"P")
    canvasPull = TCanvas("pull_plot_{}".format(trialName), "pull_plot_{}".format(trialName), 600, 600)
    canvasPull.SetLeftMargin(0.15)
    rooPlotPull.GetYaxis().SetTitleOffset(1.4)
    rooPlotPull.Draw()
    return canvasPull

def DoCorrMatPlot(rooFitRes, trialName):
    histCorrMat = rooFitRes.correlationHist("hist_corr_mat_{}".format(trialName))
    canvasCorrMat = TCanvas("corr_mat_{}".format(trialName), "corr_mat_{}".format(trialName), 600, 600)
    histCorrMat.Draw("COLZ")
    return canvasCorrMat

def DoPropagandaPlot(rooDs, pdf, rooPlot, pdfDict, histName, trialName, path, extraText):
    rooDs.plotOn(rooPlot, ROOT.RooFit.Name("Data"), ROOT.RooFit.MarkerStyle(20), ROOT.RooFit.MarkerSize(0.5))
    pdf.plotOn(rooPlot, ROOT.RooFit.Name("Fit"), ROOT.RooFit.LineColor(ROOT.kRed+1), ROOT.RooFit.LineWidth(2))
    for i in range(0, len(pdfDict["pdf"])):
        if not pdfDict["pdfName"][i] == "SUM":
            pdf.plotOn(rooPlot, ROOT.RooFit.Components("{}Pdf".format(pdfDict["pdfName"][i])), ROOT.RooFit.Name(pdfDict["pdfNameForLegend"][i]), ROOT.RooFit.LineColor(pdfDict["pdfColor"][i]), ROOT.RooFit.LineStyle(pdfDict["pdfStyle"][i]), ROOT.RooFit.LineWidth(2))
    rooPlot.SetAxisRange(0, 1.7 * rooPlot.GetMaximum(), "Y")

    legend = ROOT.TLegend(0.65, 0.93-0.05*(len(pdfDict["pdf"])+1), 0.85, 0.93, " ", "brNDC")
    legend.SetBorderSize(0)
    legend.SetFillColor(10)
    legend.SetFillStyle(1)
    legend.SetLineStyle(0)
    legend.SetLineColor(0)
    legend.SetTextFont(42)
    legend.SetTextSize(0.04)
    legend.AddEntry(rooPlot.findObject("Data"), "Data", "P")
    legend.AddEntry(rooPlot.findObject("Fit"), "Fit", "L")
    for i in range(0, len(pdfDict["pdf"])):
        if not pdfDict["pdfName"][i] == "SUM":
            legend.AddEntry(rooPlot.findObject(pdfDict["pdfNameForLegend"][i]), pdfDict["pdfNameForLegend"][i], "L")

    rooPlot.SetTitle("")

    canvasALICE = TCanvas("ALICE_{}_{}".format(histName, trialName), "ALICE_{}_{}".format(histName, trialName), 800, 600)
    canvasALICE.Update()
    canvasALICE.SetLeftMargin(0.15)
    rooPlot.Draw()

    legend.Draw("same")

    letexTitle = TLatex()
    letexTitle.SetTextSize(0.045)
    letexTitle.SetNDC()
    letexTitle.SetTextFont(42)
    for i in range(0, len(pdfDict["textForPropagandaPlot"])):
        letexTitle.DrawLatex(pdfDict["textForPropagandaPlot"][i][0], pdfDict["textForPropagandaPlot"][i][1], pdfDict["textForPropagandaPlot"][i][2])

    letexExtraText = TLatex()
    letexExtraText.SetTextSize(0.04)
    letexExtraText.SetTextColor(ROOT.kGray+3)
    letexExtraText.SetNDC()
    letexExtraText.SetTextFont(42)
    lineIndex = 0
    for line in extraText:
        letexExtraText.DrawLatex(0.65, 0.62 - lineIndex, line)
        lineIndex = lineIndex + 0.06

    if not os.path.isdir(path):
        os.system("mkdir -p %s" % (path))

    canvasALICE.SaveAs("{}ALICE_{}_{}.pdf".format(path, histName, trialName))