using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography.X509Certificates;
using System.Windows.Forms;
using Microsoft.Win32;

[assembly: AssemblyTitle("Rezo Setup")]
[assembly: AssemblyProduct("Rezo")]
[assembly: AssemblyCompany("Pandajupiter")]
[assembly: AssemblyDescription("Rezo privacy browser - installer")]
[assembly: AssemblyCopyright("Copyright (c) Pandajupiter")]
[assembly: AssemblyVersion("1.5.3.0")]
[assembly: AssemblyFileVersion("1.5.3.0")]

class RezoSetupForm : Form
{
    TextBox log_;

    [STAThread]
    static void Main()
    {
        // Request admin elevation if not already elevated
        if (!IsRunAsAdmin())
        {
            var startInfo = new ProcessStartInfo(Assembly.GetExecutingAssembly().Location)
            {
                UseShellExecute = true,
                Verb = "runas"
            };
            try { Process.Start(startInfo); } catch { }
            return;
        }

        System.Net.ServicePointManager.SecurityProtocol =
            System.Net.SecurityProtocolType.Tls12;
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new RezoSetupForm());
    }

    static bool IsRunAsAdmin()
    {
        using (var identity = System.Security.Principal.WindowsIdentity.GetCurrent())
        {
            var principal = new System.Security.Principal.WindowsPrincipal(identity);
            return principal.IsInRole(System.Security.Principal.WindowsBuiltInRole.Administrator);
        }
    }

    RezoSetupForm()
    {
        Text = "Rezo Setup";
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        ClientSize = new System.Drawing.Size(520, 320);
        BackColor = System.Drawing.Color.Black;
        StartPosition = FormStartPosition.CenterScreen;

        log_ = new TextBox
        {
            Multiline = true,
            ReadOnly = true,
            Dock = DockStyle.Fill,
            BackColor = System.Drawing.Color.Black,
            ForeColor = System.Drawing.Color.LimeGreen,
            Font = new System.Drawing.Font("Consolas", 10),
            BorderStyle = BorderStyle.None,
            ScrollBars = ScrollBars.Vertical,
        };
        Controls.Add(log_);

        Shown += (s, e) => Run();
    }

    void Log(string line)
    {
        log_.AppendText(line + Environment.NewLine);
        log_.SelectionStart = log_.TextLength;
        log_.ScrollToCaret();
        Application.DoEvents();
        System.Threading.Thread.Sleep(60);
    }

    void Run()
    {
        string dir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        string cer = Path.Combine(dir, "Rezo-selfsign.cer");

        try
        {
            Log("> Rezo setup - fetching latest from server...");
            string tmp = Path.Combine(Path.GetTempPath(), "rezo-setup");
            Directory.CreateDirectory(tmp);
            string server = "https://github.com/0Panda-Development/rezo/releases/latest/download/";
            string ver = Fetch(server + "version.txt");
            if (ver == null)
            {
                Log("[FAIL] update server unreachable (no internet?)");
                Finish();
                return;
            }
            string msix = Path.Combine(tmp, "Rezo-" + ver + ".msix");
            cer = Path.Combine(tmp, "Rezo-selfsign.cer");
            try
            {
                Log("> downloading v" + ver + "...");
                Download(server + "Rezo-" + ver + ".msix", msix);
                Download(server + "Rezo-selfsign.cer", cer);
                Log("[OK] downloaded v" + ver);
            }
            catch (Exception e)
            {
                Log("[FAIL] download failed: " + e.Message);
                Finish();
                return;
            }
            string msixName = Path.GetFileName(msix);

            Log("> trusting Rezo publisher certificate");
            try
            {
                var store = new X509Store(StoreName.Root, StoreLocation.LocalMachine);
                store.Open(OpenFlags.ReadWrite);
                store.Add(new X509Certificate2(cer));
                store.Close();
                Log("[OK] certificate installed");
            }
            catch (Exception e)
            {
                Log("[FAIL] certificate: " + e.Message);
                Finish();
                return;
            }

            Log("> enabling app sideloading");
            try
            {
                Registry.LocalMachine.CreateSubKey(
                    @"SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock")
                    .SetValue("AllowAllTrustedApps", 1, RegistryValueKind.DWord);
                Log("[OK] sideloading enabled");
            }
            catch (Exception e)
            {
                Log("[FAIL] sideloading: " + e.Message);
                Finish();
                return;
            }

            Log("> installing Rezo (this takes a moment)...");
            var p = new Process
            {
                StartInfo = new ProcessStartInfo("powershell",
                    "-NoProfile -Command \"Add-AppxPackage -Path '" + msix + "' -ForceApplicationShutdown\"")
                {
                    UseShellExecute = false,
                    CreateNoWindow = true,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                }
            };
            p.Start();
            p.WaitForExit();
            string err = p.StandardError.ReadToEnd();

            if (p.ExitCode != 0 && !err.Contains("already installed"))
            {
                Log("[FAIL] install: " + err);
                Finish();
                return;
            }

            Log("[OK] Rezo installed successfully");
            // Overwrite old updater with new one (fixes stale URL in cached updater)
            try
            {
                string localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
                string rootDir = Path.Combine(localAppData, "Rezo");
                string newUpdater = Path.Combine(rootDir, "RezoUpdater.exe");
                if (File.Exists(newUpdater))
                    File.Delete(newUpdater);
                File.Copy(Assembly.GetExecutingAssembly().Location, newUpdater, true);
                Log("[OK] refreshed local updater");
            }
            catch { }
            Log("> launching Rezo");
            try
            {
                var ps = new Process
                {
                    StartInfo = new ProcessStartInfo("powershell",
                        "-NoProfile -Command \"$p=Get-AppxPackage Pandajupiter.Rezo; explorer ('shell:AppsFolder\\' + $p.PackageFamilyName + '!Rezo')\"")
                    {
                        UseShellExecute = false,
                        CreateNoWindow = true,
                    }
                };
                ps.Start();
            }
            catch { }
            Log(" ");
            Log("DONE - you can close this window.");
            Finish();
        }
        catch (Exception e)
        {
            Log("[FAIL] " + e.Message);
            Log(" ");
            Log("Something went wrong - copy this text and send it to the developer.");
            Finish();
        }
    }

    void Finish()
    {
        // Keep the window open so the result is visible; close manually.
    }

    string Fetch(string url)
    {
        try
        {
            var req = (System.Net.HttpWebRequest)System.Net.WebRequest.Create(url);
            req.Timeout = 8000;
            req.ReadWriteTimeout = 8000;
            req.UserAgent = "RezoSetup";
            using (var resp = (System.Net.HttpWebResponse)req.GetResponse())
            using (var r = new StreamReader(resp.GetResponseStream()))
                return r.ReadToEnd().Trim();
        }
        catch
        {
            return null;
        }
    }

    void Download(string url, string dest)
    {
        var req = (System.Net.HttpWebRequest)System.Net.WebRequest.Create(url);
        req.Timeout = 600000;
        req.ReadWriteTimeout = 600000;
        req.UserAgent = "RezoSetup";
        using (var resp = (System.Net.HttpWebResponse)req.GetResponse())
        using (var src = resp.GetResponseStream())
        using (var dst = File.Create(dest))
            src.CopyTo(dst);
    }
}