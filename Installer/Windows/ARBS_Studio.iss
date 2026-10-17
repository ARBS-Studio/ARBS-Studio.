; =============================================================================
; ARBS Studio - instaluesi i vetem per Windows (Inno Setup 6)
; Instalon add-on-in ne dosjen "Add-Ons" te cdo versioni te gjetur te Archicad-it
; (28, 29, 30). Nuk kerkon asnje program apo shtese tjeter.
;
; Kompilimi:  iscc /DAppVersion=0.1.0 /DBinDir=..\..\dist Installer\Windows\ARBS_Studio.iss
;   ku dist\AC28\ARBS_Studio.apx, dist\AC29\ARBS_Studio.apx, dist\AC30\ARBS_Studio.apx
; =============================================================================

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BinDir
  #define BinDir "..\..\dist"
#endif

[Setup]
AppId={{6B3A2F1E-5C7D-4E8A-9B21-0A4B53000001}
AppName=ARBS Studio për Archicad
AppVersion={#AppVersion}
AppPublisher=ARBS Studio
AppVerName=ARBS Studio {#AppVersion}
DefaultDirName={autopf}\ARBS Studio
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
OutputDir=..\..\dist
OutputBaseFilename=ARBS_Studio_Setup_{#AppVersion}
SetupIconFile=arbs.ico
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard_small.bmp
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\arbs.ico
CloseApplications=yes
CloseApplicationsFilter=Archicad*.exe

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"

[Messages]
en.WelcomeLabel1=Mirë se vini në instalimin e ARBS Studio
en.WelcomeLabel2=Ky instalues vendos add-on-in ARBS Studio në Archicad 28, 29 dhe 30 (versionet që gjenden në kompjuter).%n%nMbyllni Archicad-in para se të vazhdoni.
en.SelectComponentsLabel2=Zgjidhni versionet e Archicad-it ku do të instalohet ARBS Studio:
en.FinishedLabel=ARBS Studio u instalua. Hapni Archicad-in dhe zgjidhni menynë «ARBS Studio › Paleta ARBS Studio».

[Types]
Name: "auto"; Description: "Versionet e gjetura të Archicad-it"
Name: "custom"; Description: "Zgjedhje manuale"; Flags: iscustom

[Components]
Name: "ac28"; Description: "Archicad 28"; Types: auto custom; Check: HasArchicad('28')
Name: "ac29"; Description: "Archicad 29"; Types: auto custom; Check: HasArchicad('29')
Name: "ac30"; Description: "Archicad 30"; Types: auto custom; Check: HasArchicad('30')
Name: "copy"; Description: "Kopje rezervë në Dokumentet publike (për instalim manual me Add-On Manager)"; Types: auto custom

[Files]
Source: "{#BinDir}\AC28\ARBS_Studio.apx"; DestDir: "{code:AcDir|28}\Add-Ons\ARBS Studio"; Components: ac28; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BinDir}\AC29\ARBS_Studio.apx"; DestDir: "{code:AcDir|29}\Add-Ons\ARBS Studio"; Components: ac29; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BinDir}\AC30\ARBS_Studio.apx"; DestDir: "{code:AcDir|30}\Add-Ons\ARBS Studio"; Components: ac30; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BinDir}\AC28\ARBS_Studio.apx"; DestDir: "{commondocs}\ARBS Studio\Archicad 28"; Components: copy; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BinDir}\AC29\ARBS_Studio.apx"; DestDir: "{commondocs}\ARBS Studio\Archicad 29"; Components: copy; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#BinDir}\AC30\ARBS_Studio.apx"; DestDir: "{commondocs}\ARBS Studio\Archicad 30"; Components: copy; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\..\Docs\Udhezuesi.pdf"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "arbs.ico"; DestDir: "{app}"; Flags: ignoreversion

[Code]
var
  FoundDirs: array[0..2] of String;

function CandidateDir(Ver: String): String;
var
  Bases: array[0..3] of String;
  Names: array[0..3] of String;
  i, j: Integer;
  RegPath: String;
begin
  Result := '';
  { 1) regjistri: celesat e instalimit te Graphisoft }
  if RegQueryStringValue(HKLM64, 'SOFTWARE\GRAPHISOFT\Archicad ' + Ver, 'InstallLocation', RegPath) and DirExists(RegPath) then
  begin
    Result := RegPath;
    exit;
  end;
  { 2) dosjet standarde }
  Bases[0] := ExpandConstant('{commonpf64}') + '\Graphisoft';
  Bases[1] := ExpandConstant('{commonpf64}') + '\GRAPHISOFT';
  Bases[2] := 'C:\Program Files\Graphisoft';
  Bases[3] := 'D:\Program Files\Graphisoft';
  Names[0] := 'Archicad ' + Ver;
  Names[1] := 'ARCHICAD ' + Ver;
  Names[2] := 'Archicad ' + Ver + ' INT';
  Names[3] := 'Archicad ' + Ver + ' Solo';
  for i := 0 to 3 do
    for j := 0 to 3 do
      if DirExists(Bases[i] + '\' + Names[j] + '\Add-Ons') then
      begin
        Result := Bases[i] + '\' + Names[j];
        exit;
      end;
end;

function IndexOf(Ver: String): Integer;
begin
  if Ver = '28' then Result := 0
  else if Ver = '29' then Result := 1
  else Result := 2;
end;

function HasArchicad(Ver: String): Boolean;
begin
  if FoundDirs[IndexOf(Ver)] = '' then
    FoundDirs[IndexOf(Ver)] := CandidateDir(Ver);
  Result := FoundDirs[IndexOf(Ver)] <> '';
end;

function AcDir(Ver: String): String;
begin
  HasArchicad(Ver);
  Result := FoundDirs[IndexOf(Ver)];
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not (HasArchicad('28') or HasArchicad('29') or HasArchicad('30')) then
    MsgBox('Nuk u gjet asnjë instalim i Archicad 28/29/30 në dosjet standarde.' + #13#10 +
           'ARBS Studio do të kopjohet në Dokumentet publike › ARBS Studio; ngarkojeni me Archicad › Opsionet › Add-On Manager.',
           mbInformation, MB_OK);
end;
