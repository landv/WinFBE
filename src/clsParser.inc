'    WinFBE - Programmer's Code Editor for the FreeBASIC Compiler
'    Copyright (C) 2016-2022 Paul Squires, PlanetSquires Software
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

#include once "clsParser.bi"

' ========================================================================================
' 
' ========================================================================================
function AfxStrRemoveWithMark( _
            byref wszMainStr as const wstring, _
            byref wszDelim1 as const wstring, _
            byref wszDelim2 as const wstring, _
            byref MarkKeys as const wstring = "", _
            byval fRemoveAll as boolean = false , _
            byval IsInstrRev as boolean = false  _
            ) as CWSTR
   dim nPos1 as long =iif(IsInstrRev = true, instrrev(wszMainStr, wszDelim1), instr(wszMainStr, wszDelim1))
   if nPos1 = 0 then return wszMainStr
   dim nPos2 as long = instr(nPos1 + len(wszDelim1), wszMainStr, wszDelim2)
   if nPos2 = 0 then return wszMainStr
   nPos2 += len(wszDelim2)
   dim nLen as long = nPos2 - nPos1
   if fRemoveAll = false  then 
      return mid(wszMainStr, 1, nPos1 - 1)& MarkKeys & mid(wszMainStr, nPos2)
   else
      return AfxStrRemoveWithMark(mid(wszMainStr, 1, nPos1 - 1) & MarkKeys & mid(wszMainStr, nPos2), wszDelim1, wszDelim2,MarkKeys, fRemoveAll,IsInstrRev)  
   end if
end function


' ========================================================================================
' Get the details of a TODO item
' ========================================================================================
function clsParser.parseToDoItem( byval sText as string ) as boolean
   dim as long nPos
   nPos = instr(ucase(sText), "' TODO:")
   if nPos = 0 then nPos = instr(ucase(sText), "'TODO:")
   if nPos = 0 then return false 
   ' Everything leading up to the found "'" must be a space. Use pointer
   ' indexing for speed to bypass expensive ltrim, etc.
   ' convert to zero base and test up to char immediately before "'"
   for i as long = 0 to nPos - 2    
      if sText[i] <> 32 then return false 
   next
   this.todoText = mid(sText, nPos + 7)
   gdb2.dbAdd( @this, DB2_TODO)
   return true
end function
   
' ========================================================================================
' Determine if we are inside a multiline comment block
' ========================================================================================
function clsParser.IsMultilineComment( byval sLine as string ) as boolean
   ' if we are already in a comment then test to see if we have now reached the end of it.
   if this.bInMultiLineComment then 
      if right(sLine, 2) = "'/" then 
         this.bInMultiLineComment = false 
      end if
   else   
      ' It is possible that the start and end comment markers may appear on 
      ' the same line. Do a test for this.
      if (left(sLine, 2) = "/'") andalso (right(sLine, 2) <> "'/") then 
         this.bInMultiLineComment = true
      else
         ' Not in a multiline comment. Just a normal line.
         return false          
      end if
   end if
   return true
end function


' ========================================================================================
' Normalize the line by removing double spaces, tabs, etc
' ========================================================================================
function clsParser.NormalizeLine() as boolean
   
   ' Remove double spaces and replace TABs with single space
   this.st = AfxStrShrink(this.st, chr(32,9))

   ' Also save our ucase version of the line
   this.st_ucase = ucase(this.st)
   
   ' return TRUE if length of resulting line is zero.
   if len(this.st) = 0 then return true
end function


' ========================================================================================
' Inspect the line for start/end points of the current parser action
' ========================================================================================
function clsParser.InspectLine() as boolean
   ' Inspect the current line for any start/end points of an existing parse
   ' and take the appropriate action.
   ' Will return TRUE is the inspection changed the state of the current
   ' parser action.
   dim as long i, nCount
   dim as string sTemp
   
   ' CHECK FOR THE START OF A TYPE STRUCTURE.
   if gConfig.CodeTips then
      if len(st_ucase) > 5 then 
         if (left(st_ucase, 5) = "TYPE ") then
            ' if " as " exists then this is not a TYPE structure but rather an ALIAS 
            ' for another TYPE.
            i = instr(st_ucase, " AS ") 
            if i then 
               this.typeName  = trim(mid(st, 6, (i-6)))
               this.typeAlias = trim(AfxStrRemainI(st, " as "))
               ' Determine if this is an ALIAS for a function or Sub similar to the following example:
               ' Type Scintilla_Directfunction as function CDECL (byval as Any Ptr, byval as UINT, byval as WPARAM, byval as LPARAM ) as LRESULT
               if left(ucase(this.typeAlias), 8) = "FUNCTION" then
                  this.typeAlias = this.typeName
               end if
               gdb2.dbAdd( @this, DB2_TYPE )
               this.typeName  = "": this.typeAlias = ""
               this.funcLineNum = this.lineNum
               return true
            else
               ' We have encountered the start of a TYPE structure. Save the TYPE name.
               ' Syntax:   TYPE typename [Extends base_typename] [Field = alignment]
               this.action = ACTION_PARSETYPE
               this.typeName  = AfxStrParse(st, 2, " ")
               this.typeAlias = this.typeName
               ' Handle case where this Type extends from another Type
               i = instr(st_ucase, " EXTENDS") 
               if i then 
                  sTemp = AfxStrParse(st, 4, " ")
                  if ucase(sTemp) <> "OBJECT" then
                     this.TypeExtends = sTemp
                  end if   
               end if
               this.bInTypePublic = true    ' assume that all elements in TYPE are public
               this.IsEnum = false 
               this.funcLineNum = this.lineNum
               return true
            end if

         ' CHECK FOR THE END OF A TYPE STRUCTURE
         elseif (left(st_ucase, 8) = "END TYPE") and (gConfig.CodeTips) then
            gdb2.dbAdd( @this, DB2_TYPE )
            this.action = ACTION_NONE
            this.typeName  = ""
            this.typeAlias = ""
            return true
         end if   
      end if
   end if
   
   ' Check for any SUB/function blocks of code. Bypass doing this test
   ' if the line length is less than the minimum (4) because in that case we
   ' know it can not be any of these so why even waste time testing them.
   
   ' TEST FOR START OF SUB/FUNCTION
   ' These can be part of a TYPE or standalone.
   
   sTemp = ""
   if len(st_ucase) > 4 then 
      if left(st_ucase, 7)  = "STATIC " then
         st = mid(st, 8): st_ucase = ucase(st)
      end if   
      if left(st_ucase, 4)  = "SUB " then
         sTemp = mid(st, 5): this.action = ACTION_PARSESUB
      elseif left(st_ucase, 9)  = "FUNCTION " then
         ' bypass any line with = that is a "function = " line.
         dim as string subText = ltrim( mid(st, 9) )
         if left( subText, 1 ) <> "=" then 
            sTemp = mid(st, 10)
            this.action = ACTION_PARSEFUNCTION
         end if   
      elseif left(st_ucase, 16) = "PUBLIC FUNCTION " then
         sTemp = mid(st, 17): this.action = ACTION_PARSEFUNCTION
      elseif left(st_ucase, 17) = "PRIVATE FUNCTION " then
         sTemp = mid(st, 18): this.action = ACTION_PARSEFUNCTION
      elseif left(st_ucase, 11) = "PUBLIC SUB " then
         sTemp = mid(st, 12): this.action = ACTION_PARSESUB
      elseif left(st_ucase, 12) = "PRIVATE SUB " then
         sTemp = mid(st, 13): this.action = ACTION_PARSESUB
      elseif left(st_ucase, 9) = "PROPERTY " then
         ' bypass any line with = that is a "PROPERTY = " line.
         dim as string subText = ltrim( mid(st, 9) )
         if left( subText, 1 ) <> "=" then 
            sTemp = mid(st, 10)
            this.action = ACTION_PARSEPROPERTY
         end if   
      elseif left(st_ucase, 17) = "PRIVATE PROPERTY " then
         sTemp = mid(st, 18): this.action = ACTION_PARSEPROPERTY
      elseif left(st_ucase, 12) = "CONSTRUCTOR " then
         sTemp = mid(st, 13): this.action = ACTION_PARSECONSTRUCTOR
      elseif left(st_ucase, 11) = "DESTRUCTOR " then
         sTemp = mid(st, 12): this.action = ACTION_PARSEDESTRUCTOR
      end if
   end if   
   
   if len(sTemp) then 
      ' bypass any line with = embedded in it. Because the function keyword has now
      ' been removed from the line, it is easy to check if the "=" sign is the next character.
      if sTemp[0] = 61 then return true   ' 61 is "="
      ' Save data values for later until the end of the block is encountered.
      dim as string sFuncParams, st
      this.funcName = sTemp
      i = instr(sTemp, "(")
      if i then sFuncParams = mid(sTemp, i)
      if i then this.funcName = rtrim(left(sTemp, i-1))
      
      ' The sub/function name will be up to the first whitespace
      dim as long ii = instr(this.funcName, " ")
      if ii then this.funcName = left( this.funcName, ii-1 )

      ' if this is a PROPERTY then we need to make an educated assessment of whether
      ' it is a Get or Set property and set the flag appropriately.
      if this.action = ACTION_PARSEPROPERTY then
         ' if funcParams exist then this must be a Set property. Need to sanitize the
         ' property parameters first. Need to get the starting ( and the ending ) and
         ' evaluate the text between it. Not as easy as using Parse because there could
         ' be embedded array() parameters and the property could end in something like 
         ' as long, etc.
         dim as long p1, p2
         p1 = instr( sFuncParams, "(" )
         p2 = InstrRev( sFuncParams, ")" )
         if ( p1 = 0 ) orelse (p2 = 0 ) then
            st = ""
         elseif p2 > p1 then
            st = mid( sFuncParams, p1, p2 - p1 )
         end if
         st = trim( st, any "( )" )
         
         if len( st ) then
            this.GetSet = ClassProperty.Setter
         else   
            this.GetSet = ClassProperty.Getter
         end if

      elseif this.action = ACTION_PARSECONSTRUCTOR then
         this.GetSet = ClassProperty.ctor

      elseif this.action = ACTION_PARSEDESTRUCTOR then
         this.GetSet = ClassProperty.dtor

      else      
         this.GetSet = ClassProperty.None
      end if

      if i then this.funcParams = this.funcName & sFuncParams
      this.funcLineNum = this.lineNum
      
      return true
   end if

   ' TEST FOR END OF SUB/function BLOCK
   if len(st_ucase) >= 7 then 
      if (left(st_ucase, 7) = "END SUB") orelse _
         (left(st_ucase, 12) = "END FUNCTION") orelse _
         (left(st_ucase, 12) = "END PROPERTY") orelse _
         (left(st_ucase, 15) = "END CONSTRUCTOR") orelse _
         (left(st_ucase, 14) = "END DESTRUCTOR") then 

         ' Add the function to the database
         if this.bParsingCodeGen = false  then
            select case this.action
               CASE ACTION_PARSEFUNCTION:    gdb2.dbAdd( @this, DB2_function )
               CASE ACTION_PARSESUB:         gdb2.dbAdd( @this, DB2_SUB )
               CASE ACTION_PARSEPROPERTY:    gdb2.dbAdd( @this, DB2_PROPERTY )
               CASE ACTION_PARSECONSTRUCTOR: gdb2.dbAdd( @this, DB2_PROPERTY ) ' lump in with properties
               CASE ACTION_PARSEDESTRUCTOR:  gdb2.dbAdd( @this, DB2_PROPERTY ) ' lump in with properties
            END SELECT
         end if
         
         if gConfig.CodeTips then
            ' Need to parse any function parameters to ensure that they get added
            ' as variables in the database. 
            ' Isolate the parameter list from the codetip
            sTemp = trim(AfxStrExtract ( this.funcParams, "(", ")" ))

            ' Parse the function parameters
            if len(sTemp) then 
               this.action = ACTION_PARSEPARAMETERS
               nCount = AfxStrParseCount(sTemp, ",")
               dim as long tlineNum = this.lineNum
               this.lineNum = this.funcLineNum
               for i = 1 to nCount
                  this.st = trim(AfxStrParse(sTemp, i, ","))
                  this.st_ucase = ucase(this.st)
                  this.parseVariableDefinitions
               next
               this.lineNum=tlineNum
            end if
         end if
         
         ' Reset our function variables
         this.action = ACTION_NONE
         this.funcName = ""
         this.funcParams = ""
         return true
      end if
   end if

   return false 
end function


' ========================================================================================
' Determine if the variable is one of the predefined standard data types
' ========================================================================================
function clsParser.IsStandardDataType( byref sVarType as string ) as boolean
   static as string sList 
   sList = " boolean byte double integer long longint short single" & _
           " string ubyte uinteger ushort wstring zstring cwstr "
   if instr(sList, " " & lcase(sVarType) & " " ) then return true
   return false  
end function


' ========================================================================================
' Parse a dim that has multiple variables
' ========================================================================================
function clsParser.parseVariableDefinitions() as boolean
   if gConfig.CodeTips = false then return false 
   if len(this.st) < 6 then return false   ' minimum line length needed "AS " (see below)
   if this.action = ACTION_PARSEENUM then return false 
   
   ' DIFFERENT WAYS TO DIMENSION VARIABLES
   ' dim shared va1 as long
   ' dim shared as long var1, var2, var3
   ' dim var1 as long
   ' dim byref var1 as 
   ' dim as long var1, var2, var3
   ' as long var1, var2, var3
   ' as long Ptr var1, var2, var3
   ' as L v    ' minimum
   ' var1 as long
   
   dim as string sTemp, sTemp_ucase, sList
   dim as long n, n2, numParts

   if this.action = ACTION_PARSETYPE then
      if left(this.st_ucase, 3) = "AS " then
         ' This type of declare syntax can only be valid inside a class/TYPE
         ' the word following is the variable type.
         ' as DataType fieldname [= initializer], ...
         ' as DataType fieldname(array dimensions) [= initializer], ...
         ' as DataType fieldname(Any [, Any...])
         ' as DataType fieldname : bits [= initializer], ...
         ' as DataType fieldname3, fieldname4
         sTemp = this.st
         
      elseif instr(this.st_ucase, " AS ") then
         ' This is the normal way variables are defined in a TYPE. Make sure
         ' to restrict this test to TYPES only because otherwise we'll pick up
         ' statements like "For i as long = 1 to 10 
         ' fieldname1 as DataType
         ' fieldname2 as DataType
         sTemp = this.st
      end if

   elseif this.action = ACTION_PARSEPARAMETERS then
      ' Sub/function parameters appear 
      ' MyFunc( byval fieldname1 as DataType = 0 ) as long
      sTemp = this.st
      sTemp = AfxStrRemoveI(sTemp, "byval ")
      sTemp = AfxStrRemoveI(sTemp, "byref ")
      sTemp = AfxStrRemoveI(sTemp, "const ")

   else
      ' The following are the normal ways that variables are defined   
      if left(this.st_ucase, 11) = "DIM SHARED " then
         sTemp = mid(this.st, 12)
      elseif left(this.st_ucase, 10) = "DIM BYREF " then
         sTemp = mid(this.st, 11)
      elseif left(this.st_ucase, 4) = "DIM " then
         sTemp = mid(this.st, 5)
      elseif left(this.st_ucase, 13) = "REDIM SHARED " then
         sTemp = mid(this.st, 14)
      elseif left(this.st_ucase, 15) = "REDIM PRESERVE " then
         return false 
      elseif left(this.st_ucase, 6) = "REDIM " then 
         if instr(this.st_ucase, " AS ") = 0 then return false 
         sTemp = mid(this.st, 7)
      end if
   end if
   

   ' At this point the line (sTemp) has been reduced to one the two following
   ' types of declare lines, or zero length if no declare found.
   ' as long var1, var2, var3
   ' as long var1(0 to ub)
   ' as long Ptr var1, var2, var3
   ' var1 as long
   ' var1 as long = 12345    ' need to remove initializers
   ' var1 as long Pointer    ' need to remove pointer/ptr
   if len(sTemp) = 0 then return false 

   ' Remove any text and array parenthesis.
   ' For example: 
   ' as wstring * MAX_PATH wszText = myFunc("FFFF"), wszName = "TTTT", myArray(5)
   sTemp = AfxStrRemove(sTemp, """", """", true ) 
   sTemp = AfxStrRemoveWithMark(sTemp, "(", ")","",true,true) 
   sTemp = AfxStrRemoveWithMark(sTemp, "{", "}","",true,true) 
   sTemp = AfxStrRemoveWithMark(sTemp, "[", "]","",true,true) 

   ' Remove any initializers 
   n = instr(sTemp, "=")
   if n then
      sTemp = AfxStrRemoveWithMark(sTemp, "=", ",", ",", true, false ) 
      n = instr(sTemp, "=")
      if n then 
         sTemp = rtrim(left(sTemp, n-1))
      end if
   end if

   ' Remove all Pointer references (we don't need them)
   sTemp = AfxStrRemoveI(sTemp, " ptr")
   sTemp = AfxStrRemoveI(sTemp, " pointer")
             
   if len(sTemp) = 0 then return false 

   sTemp_ucase = ucase(sTemp)
    
   this.bIsAlias = false 
   
   ' "AS long var1, var2, var3"
   '  12345678
   if left(sTemp_ucase, 3) = "AS " then
      ' Get the type of variable. if is a standard variable type then we
      ' will only save the dim if it is part of a TYPE because those are 
      ' the only ones that matter when doing autocomplete. 
      n = instr(4, sTemp, " ")
      this.varType = mid(sTemp, 4, n-4) 
      ' next is a comma separated list of elements DIM'd as the datatype
      sList = mid(sTemp, n)
      n = instr(sList, ",")
      if n then
         n = instrrev(sList, " ",n)
         if n then sList = mid(sList, n+1)
      ELSE
         n = instrrev(sList, " ") 
         if n then sList = mid(sList, n+1) 
      end if
      numParts = AfxStrParseCount(sList, ",")
      for i as long = 1 to numParts
         this.varName = trim(AfxStrParse(sList, i, ","), any " :")
         if this.action = ACTION_PARSETYPE then 
            gdb2.dbAdd( @this, DB2_VARIABLE )
         else
            if IsStandardDataType(this.varType) = false  then 
               gdb2.dbAdd( @this, DB2_VARIABLE )
               return true
            end if
         end if
      next
      return true
   
   elseif instr(sTemp, ",") then
      'For example: "S1 as long , S2 as boolean , ZZ as wstring * MAX_PATH"
      ' This type of dim will not appear in TYPE structures.
      numParts = AfxStrParseCount(sTemp, ",")
      for i as long = 1 to numParts
         sList= trim(AfxStrParse(sTemp, i, ","), any " :")
         this.varName = AfxStrParse(sList, 1, " ") 
         this.varType = trim(AfxStrParse(sList, 3, " "), any " :") 
         if IsStandardDataType(this.varType) = false  then 
            gdb2.dbAdd( @this, DB2_VARIABLE )
            return true
         end if
      next
      return true
   
   else
      ' "var1 as long"
      ' This type of dim can appear in TYPE structures.
      this.varName = AfxStrParse(sTemp, 1, " ") 
      this.varType = trim(AfxStrParse(sTemp, 3, " "), any " :") 
      if this.action = ACTION_PARSETYPE then 
         gdb2.dbAdd( @this, DB2_VARIABLE )
      else
         if IsStandardDataType(this.varType) = false  then 
            gdb2.dbAdd( @this, DB2_VARIABLE )
            return true
         end if
      end if
   end if

   function = false 
end function


' ========================================================================================
' Parse a TYPE/Class structure
' ========================================================================================
function clsParser.parseTYPE() as boolean
   if gConfig.CodeTips = false  then return false 

   ' We are inside of a TYPE structure. Need to examine all lines that are marked
   ' as public in order to identify the TYPE elements.
   if this.action <> ACTION_PARSETYPE then return false 

   ' This function will also parse for SUB/FUNCTIONS; Any variables will be identified through
   ' the subsequent parser calls to clsParser.parseVariableDefinitions
   
   if st_ucase[0] = 35 then return true   ' skip any embedded conditional "#" defines
   
   ' Determine if we are switching PRIVATE / PUBLIC accesses
   if len(this.st_ucase) >= 7 then 
      if left(this.st_ucase, 8) = "PRIVATE:" then 
         this.bInTypePublic = false : return true
      elseif left(this.st_ucase, 7) = "PUBLIC:" then 
         this.bInTypePublic = true: return true
      elseif left(this.st_ucase, 10) = "PROTECTED:" then 
         this.bInTypePublic = true: return true
      end if
   end if
   
   ' Are we looking a declare line in the TYPE structure? if yes, then signal to
   ' the calling code that we can continue onwards to the next line of code and
   ' bypass any further processing of this line. The actual names and parameters
   ' are parsed later when the parser encounters the actual body code of the type/class.
   ' Simply look for 'DECLARE' because that would handle FUNCTION/SUB/PROPERTY/OPERATOR.
   if left(st_ucase, 8) = "DECLARE " then return true

   return false 
end function


' ========================================================================================
' Parse an ENUM structure
' ========================================================================================
function clsParser.parseENUM() as boolean
   if gConfig.CodeTips = false  then return false 
   ' We are inside of an ENUM structure. 
   
   ' CHECK FOR THE START OF AN ENUM STRUCTURE.
   if len(st_ucase) > 3 then 
      if left(st_ucase, 5) = "ENUM " or st_ucase = "ENUM" then
         ' An ENUM can have a name associated with it and also the "explicit" keyword
         ' eg.  ENUM myEnum Explicit
         ' Therefore, the ENUM name is always Parse = 2
         ' We have encountered the start of an ENUM structure. Save the ENUM name.
         ' if no name then skip processing the ENUM.
         action = ACTION_PARSEENUM
         this.typeName  = AfxStrParse(st, 2, " ")
         this.typeAlias = this.typeName
         this.IsEnum = true
         this.bInTypePublic = true    ' assume that all elements in ENUM are public
         this.funcLineNum = this.lineNum
         return true

      ' CHECK FOR THE END OF AN ENUM STRUCTURE
      elseif left(st_ucase, 8) = "END ENUM" then
         if len(this.typeName) then
            gdb2.dbAdd( @this, DB2_TYPE )   ' we save ENUM the same way as TYPEs
         end if
         this.action = ACTION_NONE
         this.typeName  = ""
         this.typeAlias = ""
         return true
      end if   
   end if
   
   if this.action <> ACTION_PARSEENUM then return false 

   ' Only continue to parse this ENUM if this is a named ENUM.
   if len(this.typeName) = 0 then return false 
   
   ' An ENUM can have elements on a single line or comma separated. Each element can then be
   ' further broken down as <element> = <value>

   dim as string sTemp
   dim as long nCount = AfxStrParseCount(st, ",")
   
   for i as long = 1 to nCount
      ' Get the parse piece
      sTemp = trim(AfxStrParse(st, i, ","))
      if len(sTemp) then
         ' Get the first parse because we don't need the = <value> piece
         this.varName = trim(AfxStrParse(sTemp, 1, "=") )
         this.varType = "Long"
         gdb2.dbAdd( @this, DB2_VARIABLE )
      end if
   next
   
   return false  

end function


