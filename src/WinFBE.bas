' ========================================================================================
' WinFBE
' Windows FreeBASIC Editor (Windows 32/64 bit)
' Paul Squires (2016-2018)
' ========================================================================================

'    WinFBE - Programmer's Code Editor for the FreeBASIC Compiler
'    Copyright (C) 2016-2018 Paul Squires, PlanetSquires Software
'
'    This program is free software: you can redistribute it and/or modify
'    it under the terms of the GNU General Public License as published by
'    the Free Software Foundation, either version 3 of the License, or
'    (at your option) any later version.
'
'    This program is distributed in the hope that it will be useful,
'    but WITHOUT any WARRANTY; without even the implied warranty of
'    MERCHANTABILITY or FITNESS for A PARTICULAR PURPOSE.  See the
'    GNU General Public License for more details.

#Define UNICODE
#Define _WIN32_WINNT &h0602  

#Include Once "windows.bi"
#Include Once "vbcompat.bi"
#Include Once "win\shobjidl.bi"
#Include Once "win\TlHelp32.bi"
#Include Once "crt\string.bi"
#Include Once "win\Shlobj.bi"
#Include Once "Afx\CWindow.inc"
#Include Once "Afx\AfxStr.inc"
#Include Once "Afx\AfxTime.inc"
#Include Once "Afx\AfxGdiplus.inc"
#Include Once "Afx\AfxMenu.inc" 
#Include Once "Afx\AfxCom.inc" 

Using Afx

#Define APPNAME       WStr("WinFBE - FreeBASIC Editor")
#Define APPNAMESHORT  WStr("WinFBE")
#Define APPVERSION    WStr("1.5.7") 

'#Define USE_VISUAL_DESIGNER 1

#Include Once "modScintilla.bi"
#Include Once "modDeclares.bi"         ' TYPES, DEFINES, etc
#Include Once "modDeclares_auto.bi"    ' Autogenerated by WinFBE (sub/function definitions)
#Include Once "modDB2.inc"
#Include Once "clsParser.inc"
#Include Once "clsConfig.inc"
#Include Once "modRoutines.inc"
#Include Once "modSplitter.inc" 
#Include Once "modCBColor.inc"
#Include Once "clsCollection.inc"
#Include Once "clsDocument.inc"
#Include Once "clsProject.inc"
#Include Once "clsApp.inc"
#Include Once "clsTopTabCtl.inc"
#Include Once "clsLasso.inc"
#Include Once "modVDControls.inc"
#Include Once "modVDDesignForm.inc"
#Include Once "modVDDesignFrame.inc"
#Include Once "modVDDesignMain.inc"
#Include Once "modParser.inc"
#Include Once "modAutoInsert.inc"
#Include Once "modHelp.inc"
#Include Once "modCompile.inc"
#Include Once "modMenus.inc"
#Include Once "modToolbar.inc"
#Include Once "modMRU.inc"
#Include Once "modCodetips.inc"

#Include Once "frmHotImgBtn.inc"
#Include Once "frmHotTxtBtn.inc"
#Include Once "frmRecent.inc" 
#Include Once "frmExplorer.inc" 
#Include Once "frmUserTools.inc" 
#Include Once "frmCompileConfig.inc" 
#Include Once "frmOutput.inc" 
#Include Once "frmOptionsGeneral.inc"
#Include Once "frmOptionsEditor.inc"
#Include Once "frmOptionsColors.inc"
#Include Once "frmOptionsCompiler.inc"
#Include Once "frmOptionsLocal.inc"
#Include Once "frmOptionsKeywords.inc"
#Include Once "frmOptions.inc"
#Include Once "frmTemplates.inc"
#Include Once "frmFnList.inc"
#Include Once "frmGoto.inc"
#Include Once "frmCommandLine.inc"
#Include Once "frmFindInFiles.inc"
#Include Once "frmFindReplace.inc"
#Include Once "frmProjectOptions.inc"
#Include Once "frmVDToolbox.inc"
#Include Once "frmMain.inc"


' ========================================================================================
' WinMain
' ========================================================================================
Function WinMain( ByVal hInstance     As HINSTANCE, _
                  ByVal hPrevInstance As HINSTANCE, _
                  ByVal szCmdLine     As ZString Ptr, _
                  ByVal nCmdShow      As Long _
                  ) As Long

   ' Load configuration file 
   gConfig.LoadFromFile()

   ' Load the selected localization file
   dim wszLocalizationFile as WString * MAX_PATH
   wszLocalizationFile = AfxGetExePathName + wstr("Languages\") + gConfig.LocalizationFile
   If LoadLocalizationFile(@wszLocalizationFile) = False Then
      MessageBox( 0, WStr("Localization file could not be loaded. Aborting application.") + vbcrlf + _
                   wszLocalizationFile, _
                   WStr("Error"), MB_OK Or MB_ICONWARNING Or MB_DEFBUTTON1 Or MB_APPLMODAL )
      Return 1
   End If

   ' Check for previous instance 
   If gConfig.MultipleInstances = False Then
      dim as HWND hWindow = FindWindow("WinFBE_Class", 0)
      If hWindow Then
         SetForegroundWindow(hWindow)
         frmMain_ProcessCommandLine(hWindow)
         Return True
      End If
   End If
   
   ' Initialize the COM library
   CoInitialize(Null)

   ' Load the Scintilla code editing dll
   #IfDef __FB_64BIT__
      Dim As Any Ptr pLib = Dylibload("SciLexer64.dll")
   #Else
      Dim As Any Ptr pLib = Dylibload("SciLexer32.dll")
   #EndIf
   
   ' Load the HTML help library for displaying FreeBASIC help *.chm file
   gpHelpLib = DyLibLoad( "hhctrl.ocx" )
   
   ' Load the Codetips file
   gConfig.LoadCodetips( AfxGetExePathName & "Settings\codetips.ini" )

   ' Initialize the controls in the ToolBox
   gConfig.InitializeToolBox
   
   ' Show the main form
   Function = frmMain_Show( 0, nCmdShow )

   ' Free the Scintilla library
   Dylibfree(pLib)
   
   ' Free the HTML help library
   Dylibfree(gpHelpLib)
   
   ' Uninitialize the COM library
   CoUninitialize

End Function


' ========================================================================================
' Main program entry point
' ========================================================================================
End WinMain( GetModuleHandleW(Null), Null, Command(), SW_NORMAL )





