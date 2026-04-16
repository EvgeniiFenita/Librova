using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using System.IO.Compression;
using Xunit;

namespace Librova.UI.Tests;

public sealed partial class ImportJobsServiceTests
{
    [Fact]
    public async Task Service_UsesAdaptiveWaitTimeouts_ForRollbackAndCompactionPhases()
    {
        using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        var client = new AdaptiveWaitImportJobClient();
        var service = new ImportJobsService(client);

        var result = await service.WaitForCompletionAsync(
            77UL,
            TimeSpan.FromSeconds(5),
            TimeSpan.FromMilliseconds(250),
            _ => { },
            cancellation.Token);

        Assert.NotNull(result);
        Assert.Equal(ImportJobStatusModel.Cancelled, result.Snapshot.Status);
        Assert.Equal(
            [
                TimeSpan.FromMilliseconds(250),
                TimeSpan.FromMilliseconds(250),
                TimeSpan.FromSeconds(1),
                TimeSpan.FromSeconds(2),
                TimeSpan.FromSeconds(2)
            ],
            client.WaitTimeouts);
    }

    [Fact]
    public async Task Service_PerformsEndToEndImportFlowWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-import-service",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.ServiceTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
                  <description>
                    <title-info>
                      <book-title>Понедельник начинается в субботу</book-title>
                      <author>
                        <first-name>Аркадий</first-name>
                        <last-name>Стругацкий</last-name>
                      </author>
                      <lang>ru</lang>
                    </title-info>
                  </description>
                  <body>
                    <section><p>Body</p></section>
                  </body>
                </FictionBook>
                """);

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var service = new ImportJobsService(options.PipePath);
            var jobId = await service.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [sourcePath],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.NotEqual(0UL, jobId);
            Assert.True(await service.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var result = await service.TryGetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.NotNull(result);
            Assert.Equal(ImportJobStatusModel.Completed, result!.Snapshot.Status);
            Assert.Equal(1UL, result.Summary!.ImportedEntries);

            Assert.True(await service.RemoveAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token));
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task Service_PerformsEndToEndDirectoryImportRecursively()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-import-service-directory",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.ServiceTests.Directory.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var importRoot = Path.Combine(sandboxRoot, "Incoming");
            Directory.CreateDirectory(Path.Combine(importRoot, "nested"));
            await File.WriteAllTextAsync(Path.Combine(importRoot, "one.fb2"), CreateFb2("One Book"));
            await File.WriteAllTextAsync(Path.Combine(importRoot, "nested", "two.fb2"), CreateFb2("Two Book"));
            await File.WriteAllTextAsync(Path.Combine(importRoot, "notes.txt"), "ignored");

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var service = new ImportJobsService(options.PipePath);
            var jobId = await service.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [importRoot],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.NotEqual(0UL, jobId);
            Assert.True(await service.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(5),
                cancellation.Token));

            var result = await service.TryGetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.NotNull(result);
            Assert.Equal(ImportJobStatusModel.Completed, result!.Snapshot.Status);
            Assert.Equal(ImportModeModel.Batch, result.Summary!.Mode);
            Assert.Equal(2UL, result.Summary.ImportedEntries);
            Assert.Equal(2UL, result.Summary.TotalEntries);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task Service_PerformsEndToEndMultiFileBatchImport()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-import-service-batch",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.ServiceTests.Batch.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var firstPath = Path.Combine(sandboxRoot, "first.fb2");
            var secondPath = Path.Combine(sandboxRoot, "second.fb2");
            await File.WriteAllTextAsync(firstPath, CreateFb2("First Batch Book"));
            await File.WriteAllTextAsync(secondPath, CreateFb2("Second Batch Book"));

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var service = new ImportJobsService(options.PipePath);
            var jobId = await service.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [firstPath, secondPath],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.NotEqual(0UL, jobId);
            Assert.True(await service.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(5),
                cancellation.Token));

            var result = await service.TryGetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.NotNull(result);
            Assert.Equal(ImportJobStatusModel.Completed, result!.Snapshot.Status);
            Assert.Equal(ImportModeModel.Batch, result.Summary!.Mode);
            Assert.Equal(2UL, result.Summary.ImportedEntries);
            Assert.Equal(2UL, result.Summary.TotalEntries);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task Service_ReportsRunningAggregateSnapshotCountersThroughHost()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-import-service-progress",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.ServiceTests.Progress.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var importRoot = Path.Combine(sandboxRoot, "Incoming");
            Directory.CreateDirectory(importRoot);
            const int totalBooks = 40;
            for (var index = 0; index < totalBooks; index++)
            {
                await File.WriteAllTextAsync(
                    Path.Combine(importRoot, $"book-{index:D3}.fb2"),
                    CreateFb2($"Progress Book {index:D3}"));
            }

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            await process.StartAsync(options, cancellation.Token);

            var service = new ImportJobsService(options.PipePath);
            var jobId = await service.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [importRoot],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            ImportJobSnapshotModel? runningSnapshot = null;
            var deadline = DateTime.UtcNow.AddSeconds(10);
            while (DateTime.UtcNow < deadline)
            {
                var snapshot = await service.TryGetSnapshotAsync(jobId, TimeSpan.FromSeconds(2), cancellation.Token);
                if (snapshot is not null
                    && snapshot.Status == ImportJobStatusModel.Running
                    && snapshot.TotalEntries == totalBooks
                    && snapshot.ProcessedEntries > 0
                    && snapshot.ProcessedEntries < snapshot.TotalEntries)
                {
                    runningSnapshot = snapshot;
                    break;
                }

                var result = await service.TryGetResultAsync(jobId, TimeSpan.FromSeconds(2), cancellation.Token);
                if (result is not null)
                {
                    break;
                }

                await Task.Delay(50, cancellation.Token);
            }

            Assert.NotNull(runningSnapshot);
            Assert.Equal((ulong)totalBooks, runningSnapshot!.TotalEntries);
            Assert.InRange(runningSnapshot.ProcessedEntries, 1UL, (ulong)totalBooks - 1);
            Assert.True(
                runningSnapshot.ImportedEntries + runningSnapshot.FailedEntries + runningSnapshot.SkippedEntries
                <= runningSnapshot.ProcessedEntries);

            Assert.True(await service.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(15),
                cancellation.Token));

            var finalResult = await service.TryGetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.NotNull(finalResult);
            Assert.Equal(ImportJobStatusModel.Completed, finalResult!.Snapshot.Status);
            Assert.Equal((ulong)totalBooks, finalResult.Snapshot.TotalEntries);
            Assert.Equal((ulong)totalBooks, finalResult.Snapshot.ProcessedEntries);
            Assert.Equal((ulong)totalBooks, finalResult.Summary!.TotalEntries);
            Assert.Equal((ulong)totalBooks, finalResult.Summary.ImportedEntries);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    [Fact]
    public async Task Service_ReturnsCompletedZipResult_WhenWarningsContainLongCyrillicXmlPreview()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-import-service-zip-preview",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.ServiceTests.ZipPreview.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var zipPath = Path.Combine(sandboxRoot, "problematic.zip");
            using (var stream = File.Create(zipPath))
            using (var archive = new ZipArchive(stream, ZipArchiveMode.Create))
            {
                var longTitle = string.Concat(Enumerable.Repeat("Журнал «Цигун и жизнь» ", 30));
                WriteZipEntry(
                    archive,
                    "empty-author.fb2",
                    $$"""
                    <?xml version="1.0" encoding="UTF-8"?>
                    <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
                      <description>
                        <title-info>
                          <genre>periodic</genre>
                          <author>
                            <first-name></first-name>
                            <last-name></last-name>
                          </author>
                          <book-title>{{longTitle}}</book-title>
                          <annotation><p>Проверка длинного preview с кириллицей.</p></annotation>
                          <lang>ru</lang>
                        </title-info>
                        <document-info>
                          <author>
                            <nickname>Сканировщик</nickname>
                          </author>
                        </document-info>
                      </description>
                    </FictionBook>
                    """);
            }

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(30));
            await process.StartAsync(options, cancellation.Token);

            var service = new ImportJobsService(options.PipePath);
            var jobId = await service.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [zipPath],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(await service.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(10),
                cancellation.Token));

            var result = await service.TryGetResultAsync(jobId, TimeSpan.FromSeconds(5), cancellation.Token);
            Assert.NotNull(result);
            Assert.Equal(ImportJobStatusModel.Completed, result!.Snapshot.Status);
            Assert.NotNull(result.Summary);
            Assert.Equal(1UL, result.Summary!.TotalEntries);
            Assert.Equal(1UL, result.Summary.ImportedEntries);
            Assert.Equal(0UL, result.Summary.FailedEntries);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, recursive: true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    private static string CreateFb2(string title) =>
        $$"""
        <?xml version="1.0" encoding="UTF-8"?>
        <FictionBook xmlns:l="http://www.w3.org/1999/xlink">
          <description>
            <title-info>
              <book-title>{{title}}</book-title>
              <author>
                <first-name>Arkady</first-name>
                <last-name>Strugatsky</last-name>
              </author>
              <lang>en</lang>
            </title-info>
          </description>
          <body>
            <section><p>Body</p></section>
          </body>
        </FictionBook>
        """;

    private static void WriteZipEntry(ZipArchive archive, string entryPath, string text)
    {
        var entry = archive.CreateEntry(entryPath);
        using var writer = new StreamWriter(entry.Open());
        writer.Write(text);
    }

    private sealed class AdaptiveWaitImportJobClient : ImportJobClient
    {
        private int _waitCallCount;

        public AdaptiveWaitImportJobClient()
            : base(@"\\.\pipe\Librova.UI.Tests.AdaptiveWaitImportJobs")
        {
        }

        public List<TimeSpan> WaitTimeouts { get; } = [];

        public override Task<Librova.V1.WaitImportJobResponse> WaitAsync(
            ulong jobId,
            TimeSpan timeout,
            TimeSpan waitTimeout,
            CancellationToken cancellationToken)
        {
            WaitTimeouts.Add(waitTimeout);
            _waitCallCount++;

            return Task.FromResult(new Librova.V1.WaitImportJobResponse
            {
                Completed = _waitCallCount >= 5
            });
        }

        public override Task<Librova.V1.GetImportJobSnapshotResponse> GetSnapshotAsync(
            ulong jobId,
            TimeSpan timeout,
            CancellationToken cancellationToken)
        {
            var status = _waitCallCount switch
            {
                1 => Librova.V1.ImportJobStatus.Running,
                2 => Librova.V1.ImportJobStatus.RollingBack,
                3 => Librova.V1.ImportJobStatus.Compacting,
                _ => Librova.V1.ImportJobStatus.Compacting
            };

            return Task.FromResult(new Librova.V1.GetImportJobSnapshotResponse
            {
                Snapshot = new Librova.V1.ImportJobSnapshot
                {
                    JobId = jobId,
                    Status = status,
                    Percent = 50,
                    Message = status.ToString()
                }
            });
        }

        public override Task<Librova.V1.GetImportJobResultResponse> GetResultAsync(
            ulong jobId,
            TimeSpan timeout,
            CancellationToken cancellationToken)
        {
            return Task.FromResult(new Librova.V1.GetImportJobResultResponse
            {
                Result = new Librova.V1.ImportJobResult
                {
                    Snapshot = new Librova.V1.ImportJobSnapshot
                    {
                        JobId = jobId,
                        Status = Librova.V1.ImportJobStatus.Cancelled,
                        Percent = 100,
                        Message = "Import was cancelled."
                    },
                    Summary = new Librova.V1.ImportSummary
                    {
                        Mode = Librova.V1.ImportMode.Batch
                    },
                    Error = new Librova.V1.DomainError
                    {
                        Code = Librova.V1.ErrorCode.Cancellation,
                        Message = "Import was cancelled."
                    }
                }
            });
        }
    }
}
