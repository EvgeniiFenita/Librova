using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Xunit;

namespace Librova.UI.Tests;

public sealed class LibraryCatalogServiceTests
{
    private const string PwshPath = @"C:\Program Files\PowerShell\7\pwsh.exe";

    [Fact]
    public async Task LibraryCatalogService_ListsBooksWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceTests.{Environment.ProcessId}.{Environment.TickCount64}",
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
                      <book-title>Monday Begins on Saturday</book-title>
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
                """);

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var importService = new ImportJobsService(options.PipePath);
            var jobId = await importService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [sourcePath],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(await importService.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "monday",
                    SortBy = BookSortModel.DateAdded,
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);
            Assert.Equal("Monday Begins on Saturday", page.Items[0].Title);
            Assert.Equal(BookFormatModel.Fb2, page.Items[0].Format);
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
    public async Task LibraryCatalogService_ExportsBookWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-export",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceExportTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook>
                  <description>
                    <title-info>
                      <book-title>Export Through Service</book-title>
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
                """);

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var importService = new ImportJobsService(options.PipePath);
            var jobId = await importService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [sourcePath],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(await importService.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "export",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);

            var exportPath = Path.Combine(sandboxRoot, "Exports", "ExportThroughService.fb2");
            var exportedPath = await service.ExportBookAsync(
                page.Items[0].BookId,
                exportPath,
                null,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Equal(Path.GetFullPath(exportPath), Path.GetFullPath(exportedPath!));
            Assert.True(File.Exists(exportPath));
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
    public async Task LibraryCatalogService_ExportsFb2AsEpubThroughBuiltInConverterProfile()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-export-built-in",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var scriptPath = Path.Combine(sandboxRoot, "fbc-standin.ps1");
            await File.WriteAllTextAsync(scriptPath,
                """
                $command = $args[0]
                $toFlag = $args[1]
                $outputFormat = $args[2]
                $overwrite = $args[3]
                $source = $args[4]
                $destinationDir = $args[5]
                if ($command -ne 'convert' -or $toFlag -ne '--to' -or $outputFormat -ne 'epub2' -or $overwrite -ne '--overwrite')
                {
                  Write-Error 'Unexpected built-in converter contract.'
                  exit 1
                }
                if ($source -notmatch '[\\/]+Library[\\/]+')
                {
                  Write-Error 'Export conversion source must stay inside the managed library.'
                  exit 1
                }
                if (-not (Test-Path -LiteralPath $source))
                {
                  Write-Error 'Export conversion source is missing.'
                  exit 1
                }
                New-Item -ItemType Directory -Force $destinationDir | Out-Null
                $actualPath = Join-Path $destinationDir 'generated-by-built-in.epub'
                Copy-Item -LiteralPath $source -Destination $actualPath -Force
                """);

            var options = new CoreHostLaunchOptions
            {
                ExecutablePath = CoreHostPathResolver.ResolveDevelopmentExecutablePath(Path.Combine(
                    AppContext.BaseDirectory,
                    "..",
                    "..",
                    "..",
                    "..")),
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceExportBuiltInTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew,
                ConverterMode = UiConverterMode.BuiltInFb2Cng,
                Fb2CngExecutablePath = PwshPath,
                Fb2CngConfigPath = scriptPath
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook>
                  <description>
                    <title-info>
                      <book-title>Export Through Built-In Profile</book-title>
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
                """);

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var importService = new ImportJobsService(options.PipePath);
            var jobId = await importService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [sourcePath],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(await importService.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "built-in profile",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);
            Assert.Equal(BookFormatModel.Fb2, page.Items[0].Format);

            var exportPath = Path.Combine(sandboxRoot, "Exports", "ExportThroughBuiltInProfile.epub");
            var exportedPath = await service.ExportBookAsync(
                page.Items[0].BookId,
                exportPath,
                BookFormatModel.Epub,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Equal(Path.GetFullPath(exportPath), Path.GetFullPath(exportedPath!));
            Assert.True(File.Exists(exportPath));
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
    public async Task LibraryCatalogService_LoadsAggregateLibraryStatisticsWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-statistics",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceStatisticsTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook>
                  <description>
                    <title-info>
                      <book-title>Statistics Through Service</book-title>
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
                """);

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var importService = new ImportJobsService(options.PipePath);
            var jobId = await importService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [sourcePath],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(await importService.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var coversRoot = Path.Combine(options.LibraryRoot, "Covers");
            Directory.CreateDirectory(coversRoot);
            var coverPath = Path.Combine(coversRoot, "manual-cover.png");
            await File.WriteAllTextAsync(coverPath, "stub-cover-file");
            var coverSizeBytes = (ulong)new FileInfo(coverPath).Length;
            var statistics = await service.GetLibraryStatisticsAsync(
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var expectedManagedBookSizeBytes = page.Items
                .Select(item => ResolveLibraryPath(options.LibraryRoot, item.ManagedPath))
                .Aggregate(0UL, (total, path) => total + (ulong)new FileInfo(path).Length);
            var databasePath = Path.Combine(options.LibraryRoot, "Database", "librova.db");
            var databaseSizeBytes = (ulong)new FileInfo(databasePath).Length;

            Assert.Equal(1UL, statistics.BookCount);
            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);
            Assert.True(databaseSizeBytes > 0);
            Assert.Equal(expectedManagedBookSizeBytes + coverSizeBytes + databaseSizeBytes, statistics.TotalLibrarySizeBytes);
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
    public async Task LibraryCatalogService_MovesBookToTrashWithoutGeneratedProtoTypes()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-trash",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceTrashTests.{Environment.ProcessId}.{Environment.TickCount64}",
                LibraryRoot = Path.Combine(sandboxRoot, "Library"),
                LibraryOpenMode = UiLibraryOpenMode.CreateNew
            };

            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath,
                """
                <?xml version="1.0" encoding="UTF-8"?>
                <FictionBook>
                  <description>
                    <title-info>
                      <book-title>Trash Through Service</book-title>
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
                """);

            await using var process = new CoreHostProcess();
            using var cancellation = new CancellationTokenSource(TimeSpan.FromSeconds(20));
            await process.StartAsync(options, cancellation.Token);

            var importService = new ImportJobsService(options.PipePath);
            var jobId = await importService.StartAsync(
                new StartImportRequestModel
                {
                    SourcePaths = [sourcePath],
                    WorkingDirectory = Path.Combine(sandboxRoot, "Work")
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.True(await importService.WaitAsync(
                jobId,
                TimeSpan.FromSeconds(5),
                TimeSpan.FromSeconds(2),
                cancellation.Token));

            var service = new LibraryCatalogService(options.PipePath);
            var page = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "trash",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);

            var deleteResult = await service.MoveBookToTrashAsync(
                page.Items[0].BookId,
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.NotNull(deleteResult);
            Assert.Equal(DeleteDestinationModel.RecycleBin, deleteResult!.Destination);

            var afterDelete = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Text = "trash",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            Assert.Empty(afterDelete.Items);
            Assert.Equal(0UL, afterDelete.TotalCount);
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

    private static string ResolveLibraryPath(string libraryRoot, string managedPath) =>
        Path.IsPathFullyQualified(managedPath)
            ? managedPath
            : Path.GetFullPath(Path.Combine(libraryRoot, managedPath));
}
