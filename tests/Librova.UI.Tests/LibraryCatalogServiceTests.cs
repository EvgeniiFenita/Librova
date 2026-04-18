using Google.Protobuf;
using Librova.UI.CoreHost;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Librova.UI.PipeTransport;
using Librova.V1;
using System.Buffers.Binary;
using System.IO.Pipes;
using Xunit;

namespace Librova.UI.Tests;

public sealed class LibraryCatalogServiceTests
{
    private const string PwshPath = @"C:\Program Files\PowerShell\7\pwsh.exe";
    private const uint Fnv1aOffsetBasis32 = 2166136261;
    private const uint Fnv1aPrime32 = 16777619;

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
            Assert.NotNull(page.Statistics);
            Assert.Equal(1UL, page.Statistics!.BookCount);
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
                if (-not [System.IO.Path]::IsPathFullyQualified($source))
                {
                  Write-Error 'Export conversion source must be absolute.'
                  exit 1
                }
                if ($source -match '[\\/]+Library[\\/]+Temp[\\/]+')
                {
                  Write-Error 'Export conversion source must not come from legacy Library\\Temp.'
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
    public async Task LibraryCatalogService_LoadsAggregateLibraryStatisticsFromListBooksWithoutGeneratedProtoTypes()
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
            var coversRoot = Path.Combine(options.LibraryRoot, "Objects", "99", "99");
            Directory.CreateDirectory(coversRoot);
            var coverPath = Path.Combine(coversRoot, "9999999999.cover.png");
            await File.WriteAllTextAsync(coverPath, "stub-cover-file");
            var refreshedPage = await service.ListBooksAsync(
                new BookListRequestModel
                {
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);
            var expectedManagedBookSizeBytes = page.Items
                .Select(item => ResolveManagedBookPath(options.LibraryRoot, item.BookId, item.ManagedFileName))
                .Aggregate(0UL, (total, path) => total + (ulong)new FileInfo(path).Length);
            var databasePath = Path.Combine(options.LibraryRoot, "Database", "librova.db");
            var databaseSizeBytes = (ulong)new FileInfo(databasePath).Length;

            Assert.Single(page.Items);
            Assert.Equal(1UL, page.TotalCount);
            Assert.NotNull(page.Statistics);
            Assert.Equal(1UL, page.Statistics!.BookCount);
            Assert.NotNull(refreshedPage.Statistics);
            Assert.Equal(1UL, refreshedPage.Statistics!.BookCount);
            Assert.True(databaseSizeBytes > 0);
            // Orphan object files that are not referenced by cover_path in the catalog must not affect library statistics.
            Assert.Equal(
                expectedManagedBookSizeBytes + databaseSizeBytes,
                refreshedPage.Statistics.TotalLibrarySizeBytes);
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

    private static string ResolveManagedBookPath(string libraryRoot, long bookId, string managedFileName) =>
        Path.GetFullPath(Path.Combine(
            libraryRoot,
            "Objects",
            ComputeShardBucket(bookId, 0),
            ComputeShardBucket(bookId, 8),
            managedFileName));

    private static string ComputeShardBucket(long bookId, int shift)
    {
        var hash = ComputeBookShardHash(bookId);
        return $"{(hash >> shift) & 0xff:x2}";
    }

    private static uint ComputeBookShardHash(long bookId)
    {
        var bookIdText = bookId.ToString("0000000000");
        uint hash = Fnv1aOffsetBasis32;
        foreach (var ch in bookIdText)
        {
            hash ^= ch;
            hash *= Fnv1aPrime32;
        }

        return hash;
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
            Assert.False(deleteResult.HasOrphanedFiles);

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

    [Fact]
    public async Task LibraryCatalogService_ThrowsStructuredDomainExceptionForValidationExportFailure()
    {
        var sandboxRoot = Path.Combine(
            Path.GetTempPath(),
            "librova-ui-library-service-export-validation",
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
                PipePath = $@"\\.\pipe\Librova.UI.LibraryServiceExportValidationTests.{Environment.ProcessId}.{Environment.TickCount64}",
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
                      <book-title>Validation Export</book-title>
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
                    Text = "validation export",
                    Limit = 10
                },
                TimeSpan.FromSeconds(5),
                cancellation.Token);

            var error = await Assert.ThrowsAsync<LibraryCatalogDomainException>(() =>
                service.ExportBookAsync(
                    page.Items[0].BookId,
                    "relative.epub",
                    null,
                    TimeSpan.FromSeconds(5),
                    cancellation.Token));
            Assert.Equal(LibraryCatalogErrorCodeModel.Validation, error.Error.Code);
            Assert.Contains("absolute", error.Message, StringComparison.OrdinalIgnoreCase);
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
    public async Task LibraryCatalogService_GetBookDetailsThrowsStructuredNotFoundError()
    {
        var pipeName = $"Librova.UI.LibraryCatalogServiceTests.{Environment.ProcessId}.{Environment.TickCount64}";
        var pipePath = $@"\\.\pipe\{pipeName}";

        await using var server = new NamedPipeServerStream(
            pipeName,
            PipeDirection.InOut,
            1,
            PipeTransmissionMode.Byte,
            PipeOptions.Asynchronous);

        var serverTask = Task.Run(async () =>
        {
            await server.WaitForConnectionAsync();
            var requestBytes = await ReadClientFrameAsync(server);
            var requestEnvelope = PipeProtocol.DeserializeRequestEnvelope(requestBytes);

            var responseBytes = PipeProtocol.SerializeResponseEnvelope(new PipeResponseEnvelope
            {
                RequestId = requestEnvelope.RequestId,
                Status = PipeResponseStatus.Ok,
                Payload = new GetBookDetailsResponse
                {
                    Error = new DomainError
                    {
                        Code = ErrorCode.NotFound,
                        Message = "Book 42 was not found."
                    }
                }.ToByteArray()
            });

            await WriteServerFrameAsync(server, responseBytes);
        });

        var service = new LibraryCatalogService(pipePath);
        var error = await Assert.ThrowsAsync<LibraryCatalogDomainException>(() =>
            service.GetBookDetailsAsync(42, TimeSpan.FromSeconds(5), CancellationToken.None));

        Assert.Equal(LibraryCatalogErrorCodeModel.NotFound, error.Error.Code);
        Assert.Equal("Book 42 was not found.", error.Message);
        await serverTask;
    }

    private static async Task<byte[]> ReadClientFrameAsync(PipeStream stream)
    {
        var prefix = new byte[sizeof(uint)];
        await ReadExactAsync(stream, prefix);
        var length = BinaryPrimitives.ReadUInt32LittleEndian(prefix);
        if (length == 0)
        {
            return [];
        }

        var payload = new byte[length];
        await ReadExactAsync(stream, payload);
        return payload;
    }

    private static async Task WriteServerFrameAsync(PipeStream stream, byte[] payload)
    {
        var prefix = new byte[sizeof(uint)];
        BinaryPrimitives.WriteUInt32LittleEndian(prefix, checked((uint)payload.Length));
        await stream.WriteAsync(prefix);
        if (payload.Length != 0)
        {
            await stream.WriteAsync(payload);
        }

        await stream.FlushAsync();
    }

    private static async Task ReadExactAsync(PipeStream stream, byte[] buffer)
    {
        var offset = 0;
        while (offset < buffer.Length)
        {
            var read = await stream.ReadAsync(buffer.AsMemory(offset, buffer.Length - offset));
            if (read == 0)
            {
                throw new IOException("Named pipe closed while reading test payload.");
            }

            offset += read;
        }
    }

}

