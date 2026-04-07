using Librova.UI.Logging;
using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Librova.UI.Shell;

internal enum Fb2ProbeOutcome
{
    Succeeded,
    ExecutableNotFound,
    ExecutionFailed,
    NoOutputProduced,
    TimedOut,
    ProbeError
}

internal sealed class Fb2ProbeResult
{
    public Fb2ProbeOutcome Outcome { get; init; }
    public string? Detail { get; init; }

    public static readonly Fb2ProbeResult Success = new() { Outcome = Fb2ProbeOutcome.Succeeded };

    public string BuildValidationMessage() => Outcome switch
    {
        Fb2ProbeOutcome.Succeeded => string.Empty,
        Fb2ProbeOutcome.ExecutableNotFound =>
            "Executable file not found. Verify the path is correct.",
        Fb2ProbeOutcome.ExecutionFailed when Detail is not null =>
            $"Converter probe failed (exit code {Detail}). Confirm the path points to a working fb2cng build.",
        Fb2ProbeOutcome.ExecutionFailed =>
            "Converter probe failed. Confirm the path points to a working fb2cng build.",
        Fb2ProbeOutcome.NoOutputProduced =>
            "Converter produced no output. The executable may not be a compatible fb2cng build.",
        Fb2ProbeOutcome.TimedOut =>
            "Converter probe timed out (5 s). The executable may be stuck or require input.",
        _ =>
            "An unexpected error occurred while running the converter probe."
    };
}

internal static class Fb2ConverterProbe
{
    // Minimal valid FB2 document that fb2cng can convert; mirrors the argument template from
    // ConverterCommandBuilder::CreateFb2CngProfile (convert --to epub2 --overwrite <src> <destdir>).
    private static readonly string MinimalFb2 =
        """
        <?xml version="1.0" encoding="utf-8"?>
        <FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"
                     xmlns:l="http://www.w3.org/1999/xlink">
          <description>
            <title-info>
              <genre>other</genre>
              <author><first-name>Librova</first-name><last-name>Probe</last-name></author>
              <book-title>Librova Converter Probe</book-title>
              <lang>en</lang>
            </title-info>
          </description>
          <body>
            <section><p>probe</p></section>
          </body>
        </FictionBook>
        """;

    public static async Task<Fb2ProbeResult> RunAsync(
        string executablePath,
        CancellationToken cancellationToken)
    {
        if (!File.Exists(executablePath))
        {
            return new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.ExecutableNotFound };
        }

        var probeDir = Path.Combine(Path.GetTempPath(), $"librova-probe-{Guid.NewGuid():N}");
        Directory.CreateDirectory(probeDir);

        try
        {
            var sourcePath = Path.Combine(probeDir, "probe.fb2");
            var outputDir = Path.Combine(probeDir, "out");
            Directory.CreateDirectory(outputDir);

            await File.WriteAllTextAsync(sourcePath, MinimalFb2, Encoding.UTF8, cancellationToken);

            using var process = new Process();
            process.StartInfo = new ProcessStartInfo
            {
                FileName = executablePath,
                UseShellExecute = false,
                CreateNoWindow = true
            };
            // Mirrors CConverterCommandBuilder::CreateFb2CngProfile argument template:
            // convert --to epub2 --overwrite <source> <destination_dir>
            process.StartInfo.ArgumentList.Add("convert");
            process.StartInfo.ArgumentList.Add("--to");
            process.StartInfo.ArgumentList.Add("epub2");
            process.StartInfo.ArgumentList.Add("--overwrite");
            process.StartInfo.ArgumentList.Add(sourcePath);
            process.StartInfo.ArgumentList.Add(outputDir);

            process.Start();

            using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
            timeoutCts.CancelAfter(TimeSpan.FromSeconds(5));

            try
            {
                await process.WaitForExitAsync(timeoutCts.Token);
            }
            catch (OperationCanceledException) when (timeoutCts.IsCancellationRequested && !cancellationToken.IsCancellationRequested)
            {
                try { process.Kill(entireProcessTree: true); } catch { }
                return new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.TimedOut };
            }

            if (process.ExitCode != 0)
            {
                return new Fb2ProbeResult
                {
                    Outcome = Fb2ProbeOutcome.ExecutionFailed,
                    Detail = process.ExitCode.ToString()
                };
            }

            if (Directory.GetFiles(outputDir, "*.epub").Length == 0)
            {
                return new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.NoOutputProduced };
            }

            return Fb2ProbeResult.Success;
        }
        catch (OperationCanceledException)
        {
            throw;
        }
        catch (Exception ex)
        {
            UiLogging.Warning("Converter probe threw unexpected exception. {Error}", ex.Message);
            return new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.ProbeError };
        }
        finally
        {
            try { Directory.Delete(probeDir, recursive: true); } catch { }
        }
    }
}
