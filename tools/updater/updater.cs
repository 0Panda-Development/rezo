using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Net;
using System.Reflection;
using System.Threading;
using System.Windows.Forms;

// Version metadata: gives the exe a proper Windows version resource so it
// doesn't look like a generic/unsigned throwaway (fewer AV false positives).
[assembly: AssemblyTitle("Rezo Updater")]
[assembly: AssemblyProduct("Rezo")]
[assembly: AssemblyCompany("Pandajupiter")]
[assembly: AssemblyDescription("Rezo privacy browser - updater and installer")]
[assembly: AssemblyCopyright("Copyright (c) Pandajupiter")]
[assembly: AssemblyVersion("1.5.3.0")]
[assembly: AssemblyFileVersion("1.5.3.0")]

class RezoUpdaterForm : Form
{
    const string APP_VERSION = "1.5.3";
    // Built-in update server; a rezo-update.url file next to this exe can
    // override it (e.g. self-hosted mirrors).
    const string DEFAULT_SERVER = "https://github.com/0Panda-Development/rezo/releases/latest/download/";

    static readonly string ExeDir_ = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
    static readonly string LocalAppData_ = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
    static readonly string RootDir_ = Path.Combine(LocalAppData_, "Rezo");
    static readonly string AppDir_ = Path.Combine(RootDir_, "app");
    static readonly string SelfPath_ = Path.Combine(RootDir_, "RezoUpdater.exe");
    static readonly string VersionFile_ = Path.Combine(AppDir_, "version.txt");
    static readonly string ZipPath_ = Path.Combine(ExeDir_, "rezo.zip");
    static readonly string UrlFile_ = Path.Combine(ExeDir_, "rezo-update.url");
    static readonly string StartMenuLnk_ = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "Rezo.lnk");
    static readonly string DesktopLnk_ = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "Rezo.lnk");

    TextBox log_;
    ProgressBar bar_;
    PictureBox logo_;
    bool silent_ = false;

    [STAThread]
    static void Main(string[] args)
    {
        System.Net.ServicePointManager.SecurityProtocol =
            System.Net.SecurityProtocolType.Tls12;
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new RezoUpdaterForm(args));
    }

    RezoUpdaterForm(string[] args = null)
    {
        if (args != null && args.Length > 0 && (args[0] == "-s" || args[0] == "--silent"))
            silent_ = true;

        if (!silent_)
        {
            Text = "Rezo Updater " + APP_VERSION;
            FormBorderStyle = FormBorderStyle.FixedSingle;
            MaximizeBox = false;
            ClientSize = new Size(560, 420);
            BackColor = Color.Black;
            StartPosition = FormStartPosition.CenterScreen;
            try { Icon = new Icon(Assembly.GetExecutingAssembly().GetManifestResourceStream("logo.ico")); } catch { }

            logo_ = new PictureBox { SizeMode = PictureBoxSizeMode.Zoom, Dock = DockStyle.Top, Height = 120 };
            try { logo_.Image = Image.FromStream(Assembly.GetExecutingAssembly().GetManifestResourceStream("logo.png")); } catch { }
            Controls.Add(logo_);

            bar_ = new ProgressBar { Dock = DockStyle.Bottom, Height = 18, Style = ProgressBarStyle.Continuous };
            Controls.Add(bar_);

            log_ = new TextBox
            {
                Multiline = true,
                ReadOnly = true,
                Dock = DockStyle.Fill,
                BackColor = Color.Black,
                ForeColor = Color.LimeGreen,
                Font = new Font("Consolas", 10),
                BorderStyle = BorderStyle.None,
                ScrollBars = ScrollBars.Vertical,
            };
            Controls.Add(log_);
            Controls.SetChildIndex(log_, 0);
            log_.BringToFront();

            Shown += (s, e) => Run();
        }
        else
        {
            Shown += (s, e) => { RunSilent(); Close(); };
        }
    }

    void Log(string line)
    {
        if (silent_) return;
        log_.AppendText(line + Environment.NewLine);
        log_.SelectionStart = log_.TextLength;
        log_.ScrollToCaret();
        Application.DoEvents();
        Thread.Sleep(60);
        try { File.AppendAllText(Path.Combine(RootDir_, "updater.log"), line + Environment.NewLine); } catch { }
    }

    void Run()
    {
        try
        {
            Log("> establishing uplink");
            string url = ReadUrl();
            Log("[OK] server: " + url);

            bool msix;
            string pkgVer, pkgFamily;
            msix = GetMsixPackage(out pkgVer, out pkgFamily);
            if (msix)
            {
                RunMsix(url, pkgVer, pkgFamily);
                Finish(true);
                return;
            }

            Log("> scanning local installation");
            string installed = File.Exists(VersionFile_) ? ReadVer(VersionFile_) : null;
            if (installed != null)
                Log("[OK] FOUND v" + installed);
            else
                Log("[WARN] NOT FOUND");

            Log("> checking for updates");
            string server = null;
            if (url != null)
            {
                try
                {
                    server = Fetch(url + "version.txt");
                    Log("[OK] server v" + server);
                }
                catch (Exception e)
                {
                    Log("[WARN] update server unreachable: " + e.Message);
                }
            }
            string zipVer = null;
            if (File.Exists(ZipPath_))
            {
                zipVer = ReadZipVer(ZipPath_);
                Log("[OK] local package v" + zipVer);
            }

            string target = null;
            if (zipVer != null && server != null)
                target = CompareVersions(zipVer, server) >= 0 ? zipVer : server;
            else if (zipVer != null)
                target = zipVer;
            else if (server != null)
                target = server;

            bool firstInstall = installed == null;
            if (installed == null)
            {
                if (target == null)
                {
                    Fail("no install package found (rezo.zip missing next to this exe)");
                    Finish(false);
                    return;
                }
                Log("> INSTALLING v" + target);
            }
            else if (target != null && CompareVersions(target, installed) > 0)
            {
                Log("> UPDATE AVAILABLE v" + target + " (installed v" + installed + ")");
            }
            else
            {
                Log("[OK] UP TO DATE v" + installed);
                if (!AppAlreadyRunning())
                    Launch();
                Finish(true);
                return;
            }

            string localZip = ZipPath_;
            if (zipVer != target)
            {
                Log("> downloading v" + target + " package from server");
                localZip = Path.Combine(ExeDir_, "rezo.new.zip");
                try
                {
                    Download(url + "rezo.zip", localZip);
                    Log("[OK] download complete");
                }
                catch (Exception e)
                {
                    Fail("download failed: " + e.Message);
                    Finish(false);
                    return;
                }
            }

            Log("> terminating running Rezo processes");
            int killed = KillRezo();
            Log("[OK] killed " + killed + " process(es)");
            Thread.Sleep(500);

            Log("> unpacking payload");
            bar_.Visible = true;
            string tmp = Path.Combine(RootDir_, ".stage");
            if (Directory.Exists(tmp))
                Directory.Delete(tmp, true);
            Directory.CreateDirectory(tmp);
            try
            {
                ExtractZip(localZip, tmp);
            }
            catch (Exception e)
            {
                Fail("package corrupt: " + e.Message);
                Finish(false);
                return;
            }
            if (!File.Exists(Path.Combine(tmp, "rezo.exe")) ||
                !File.Exists(Path.Combine(tmp, "libcef.dll")) ||
                !File.Exists(Path.Combine(tmp, "version.txt")) ||
                !File.Exists(Path.Combine(tmp, "tor", "tor.exe")))
            {
                Fail("package incomplete (missing core files)");
                Finish(false);
                return;
            }
            Log("[OK] payload verified");

            Log("> installing to " + AppDir_);
            string old = AppDir_ + ".old";
            if (Directory.Exists(old))
                Directory.Delete(old, true);
            if (Directory.Exists(AppDir_))
                Directory.Move(AppDir_, old);
            Directory.Move(tmp, AppDir_);
            if (Directory.Exists(old))
                Directory.Delete(old, true);
            try
            {
                string newUpdater = Path.Combine(AppDir_, "RezoUpdater.exe");
                if (File.Exists(newUpdater))
                    File.Copy(newUpdater, SelfPath_, true);
                else
                    File.Copy(Assembly.GetExecutingAssembly().Location, SelfPath_, true);
                string urlFile = Path.Combine(AppDir_, "rezo-update.url");
                if (File.Exists(urlFile))
                    File.Copy(urlFile, Path.Combine(RootDir_, "rezo-update.url"), true);
            }
            catch { }
            Log("[OK] installed v" + target);

            Log("> registering shortcuts");
            CreateShortcut(StartMenuLnk_);
            CreateShortcut(DesktopLnk_);
            Log("[OK] start menu + desktop shortcuts created");

            if (firstInstall)
            {
                Log("> pinning to taskbar");
                if (PinToTaskbar())
                    Log("[OK] PINNED");
                else
                    Log("[WARN] pin failed (use right-click -> Pin to taskbar)");
            }

            Launch();
            Finish(true);
        }
        catch (Exception e)
        {
            Fail("unexpected error: " + e.Message);
            Finish(false);
        }
    }

    void RunSilent()
    {
        try
        {
            string url = ReadUrl();

            bool msix;
            string pkgVer, pkgFamily;
            msix = GetMsixPackage(out pkgVer, out pkgFamily);
            if (msix)
            {
                RunMsixSilent(url, pkgVer, pkgFamily);
                return;
            }

            string installed = File.Exists(VersionFile_) ? ReadVer(VersionFile_) : null;
            string server = null;
            try { server = Fetch(url + "version.txt"); } catch { }
            string zipVer = null;
            if (File.Exists(ZipPath_))
                zipVer = ReadZipVer(ZipPath_);

            string target = null;
            if (zipVer != null && server != null)
                target = CompareVersions(zipVer, server) >= 0 ? zipVer : server;
            else if (zipVer != null)
                target = zipVer;
            else if (server != null)
                target = server;

            bool firstInstall = installed == null;
            if (installed == null)
            {
                if (target == null) return;
            }
            else if (target != null && CompareVersions(target, installed) > 0)
            {
                silent_ = false;
                Show();
                Run();
                return;
            }
            else
            {
                if (!AppAlreadyRunning()) Launch();
            }
        }
        catch { }
    }

    void RunMsixSilent(string url, string pkgVer, string pkgFamily)
    {
        string server = null;
        try { server = Fetch(url + "version.txt"); } catch { }
        if (server == null || CompareVersions(server, pkgVer) <= 0)
        {
            if (!AppAlreadyRunning()) LaunchMsix(pkgFamily);
            return;
        }
        silent_ = false;
        Show();
        RunMsix(url, pkgVer, pkgFamily);
    }

    void RunMsix(string url, string pkgVer, string pkgFamily)
    {
        Log("[OK] PACKAGED INSTALL v" + pkgVer);
        string server = null;
        try
        {
            server = Fetch(url + "version.txt");
            Log("[OK] server v" + server);
        }
        catch (Exception e)
        {
            Log("[WARN] update server unreachable: " + e.Message);
        }
        if (server == null || CompareVersions(server, pkgVer) <= 0)
        {
            Log("[OK] UP TO DATE v" + pkgVer);
            if (!AppAlreadyRunning()) LaunchMsix(pkgFamily);
            return;
        }
        Log("> UPDATE AVAILABLE v" + server + " (installed v" + pkgVer + ")");
        string msixPath = Path.Combine(RootDir_, "rezo.new.msix");
        try
        {
            Log("> downloading package");
            Download(url + "Rezo-" + server + ".msix", msixPath);
            Log("[OK] download complete");
        }
        catch (Exception e)
        {
            Fail("download failed: " + e.Message);
            Finish(false);
            return;
        }
        Log("> terminating packaged Rezo");
        KillMsixRezo();
        Thread.Sleep(500);
        Log("> installing package update");
        var psi = new ProcessStartInfo("powershell.exe",
            "-NoProfile -ExecutionPolicy Bypass -Command \"Add-AppxPackage -Update -ForceApplicationShutdown '" +
            msixPath + "'\"")
        { UseShellExecute = false, CreateNoWindow = true };
        using (var p = Process.Start(psi))
        {
            p.WaitForExit();
            if (p.ExitCode != 0)
            {
                Fail("package update failed (exit code " + p.ExitCode + ")");
                Finish(false);
                return;
            }
        }
        Log("[OK] installed v" + server);
        try
        {
            Download(url + "RezoUpdater.exe", SelfPath_);
            Log("[OK] updater refreshed");
        }
        catch { }
        if (!AppAlreadyRunning()) LaunchMsix(pkgFamily);
    }

    bool GetMsixPackage(out string version, out string familyName)
    {
        version = null;
        familyName = null;
        try
        {
            var psi = new ProcessStartInfo("powershell.exe",
                "-NoProfile -ExecutionPolicy Bypass -Command \"$p=Get-AppxPackage Pandajupiter.Rezo; if($p){$p.Version + '|' + $p.PackageFamilyName}\"")
            { UseShellExecute = false, RedirectStandardOutput = true, CreateNoWindow = true };
            using (var p = Process.Start(psi))
            {
                string line = p.StandardOutput.ReadToEnd().Trim();
                p.WaitForExit();
                int sep = line.IndexOf('|');
                if (sep > 0)
                {
                    version = line.Substring(0, sep);
                    familyName = line.Substring(sep + 1);
                    return true;
                }
            }
        }
        catch { }
        return false;
    }

    void LaunchMsix(string familyName)
    {
        Log("> launching Rezo");
        try
        {
            Process.Start("explorer.exe", "shell:AppsFolder\\" + familyName + "!Rezo");
        }
        catch (Exception e)
        {
            Fail("launch failed: " + e.Message);
            Finish(false);
        }
    }

    void KillMsixRezo()
    {
        foreach (string name in new[] { "rezo", "tor" })
        {
            foreach (Process p in Process.GetProcessesByName(name))
            {
                try
                {
                    if (p.MainModule.FileName.IndexOf("WindowsApps", StringComparison.OrdinalIgnoreCase) >= 0)
                    {
                        p.Kill();
                        p.WaitForExit(3000);
                    }
                }
                catch { }
            }
        }
    }

    void Launch()
    {
        Log("> launching Rezo");
        if (AppAlreadyRunning())
        {
            Log("[OK] Rezo already running - keeping it");
            return;
        }
        try
        {
            Process.Start(Path.Combine(AppDir_, "rezo.exe"));
        }
        catch (Exception e)
        {
            Fail("launch failed: " + e.Message);
            Finish(false);
        }
    }

    bool AppAlreadyRunning()
    {
        return Process.GetProcessesByName("rezo").Length > 0;
    }

    int KillRezo()
    {
        int n = 0;
        foreach (string name in new[] { "rezo", "tor" })
        {
            foreach (Process p in Process.GetProcessesByName(name))
            {
                try
                {
                    if (p.MainModule.FileName.StartsWith(RootDir_, StringComparison.OrdinalIgnoreCase))
                    {
                        p.Kill();
                        p.WaitForExit(3000);
                        n++;
                    }
                }
                catch { }
            }
        }
        return n;
    }

    void ExtractZip(string zip, string dest)
    {
        using (var archive = ZipFile.OpenRead(zip))
        {
            var entries = new ZipArchiveEntry[archive.Entries.Count];
            archive.Entries.CopyTo(entries, 0);
            int done = 0;
            foreach (var entry in entries)
            {
                string path = Path.Combine(dest, entry.FullName);
                if (entry.FullName.EndsWith("/", StringComparison.Ordinal))
                {
                    Directory.CreateDirectory(path);
                    continue;
                }
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                using (var src = entry.Open())
                using (var dst = File.Create(path))
                    src.CopyTo(dst);
                done++;
                if (!silent_)
                {
                    bar_.Value = done * 100 / entries.Length;
                    Application.DoEvents();
                }
            }
        }
    }

    string ReadZipVer(string zip)
    {
        using (var archive = ZipFile.OpenRead(zip))
        {
            var entry = archive.GetEntry("version.txt");
            if (entry == null)
                throw new InvalidDataException("no version.txt in package");
            using (var s = entry.Open())
            using (var r = new StreamReader(s))
                return r.ReadToEnd().Trim();
        }
    }

    string ReadUrl()
    {
        if (File.Exists(UrlFile_))
        {
            string raw = File.ReadAllText(UrlFile_).Trim();
            if (raw.Length != 0 && raw.StartsWith("http", StringComparison.OrdinalIgnoreCase))
            {
                if (!raw.EndsWith("/", StringComparison.Ordinal))
                    raw += "/";
                return raw;
            }
        }
        return DEFAULT_SERVER;
    }

    string Fetch(string url)
    {
        var req = (HttpWebRequest)WebRequest.Create(url);
        req.Timeout = 8000;
        req.ReadWriteTimeout = 8000;
        req.UserAgent = "RezoUpdater/" + APP_VERSION;
        using (var resp = (HttpWebResponse)req.GetResponse())
        using (var r = new StreamReader(resp.GetResponseStream()))
            return r.ReadToEnd().Trim();
    }

    void Download(string url, string dest)
    {
        var req = (HttpWebRequest)WebRequest.Create(url);
        req.Timeout = 600000;
        req.ReadWriteTimeout = 600000;
        req.UserAgent = "RezoUpdater/" + APP_VERSION;
        using (var resp = (HttpWebResponse)req.GetResponse())
        using (var src = resp.GetResponseStream())
        using (var dst = File.Create(dest))
            src.CopyTo(dst);
    }

    int CompareVersions(string a, string b)
    {
        try
        {
            var pa = a.Trim().Split('.');
            var pb = b.Trim().Split('.');
            int n = Math.Max(pa.Length, pb.Length);
            for (int i = 0; i < n; i++)
            {
                int x = i < pa.Length ? int.Parse(pa[i]) : 0;
                int y = i < pb.Length ? int.Parse(pb[i]) : 0;
                if (x != y)
                    return x.CompareTo(y);
            }
            return 0;
        }
        catch
        {
            return 0;
        }
    }

    string ReadVer(string path)
    {
        try
        {
            return File.ReadAllText(path).Trim();
        }
        catch
        {
            return null;
        }
    }

    void CreateShortcut(string lnkPath)
    {
        try
        {
            var ws = Activator.CreateInstance(Type.GetTypeFromProgID("WScript.Shell"));
            dynamic lnk = ws.GetType().InvokeMember("CreateShortcut",
                BindingFlags.InvokeMethod, null, ws, new object[] { lnkPath });
            lnk.GetType().InvokeMember("TargetPath", BindingFlags.SetProperty, null, lnk, new object[] { SelfPath_ });
            lnk.GetType().InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, lnk, new object[] { AppDir_ });
            lnk.GetType().InvokeMember("Description", BindingFlags.SetProperty, null, lnk, new object[] { "Rezo privacy browser" });
            lnk.GetType().InvokeMember("Save", BindingFlags.InvokeMethod, null, lnk, null);
        }
        catch { }
    }

    bool PinToTaskbar()
    {
        try
        {
            var shell = Activator.CreateInstance(Type.GetTypeFromProgID("Shell.Application"));
            object folder = shell.GetType().InvokeMember("Namespace",
                BindingFlags.InvokeMethod, null, shell, new object[] { Path.GetDirectoryName(StartMenuLnk_) });
            object item = folder.GetType().InvokeMember("ParseName",
                BindingFlags.InvokeMethod, null, folder, new object[] { "Rezo.lnk" });
            if (item == null)
                return false;
            item.GetType().InvokeMember("InvokeVerb",
                BindingFlags.InvokeMethod, null, item, new object[] { "taskbarpin" });
            Thread.Sleep(1500);
            return true;
        }
        catch
        {
            return false;
        }
    }

    void Fail(string msg)
    {
        if (!silent_)
            log_.AppendText("[FAIL] " + msg + Environment.NewLine);
        try { File.AppendAllText(Path.Combine(RootDir_, "updater.log"), "[FAIL] " + msg + Environment.NewLine); } catch { }
    }

    void Finish(bool ok)
    {
        if (silent_) { Application.Exit(); return; }
        bar_.Visible = false;
        Log(ok ? "[OK] REZO TERMINAL SESSION COMPLETE" : "[FAIL] operation aborted");
        var t = new System.Windows.Forms.Timer { Interval = 2500 };
        t.Tick += (s, e) => { t.Stop(); Application.Exit(); };
        t.Start();
    }
}