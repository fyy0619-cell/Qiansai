# -*- coding: utf-8 -*-
import sys, os
import win32com.client as win32
docx = os.path.abspath(sys.argv[1])
pdf = os.path.abspath(sys.argv[2])
word = win32.Dispatch('Word.Application')
word.Visible = False
try:
    doc = word.Documents.Open(docx, ReadOnly=1)
    doc.SaveAs(pdf, FileFormat=17)  # 17 = wdFormatPDF
    doc.Close(False)
    print("PDF saved:", pdf)
finally:
    word.Quit()
