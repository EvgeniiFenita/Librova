using Avalonia;
using Avalonia.Media;
using Librova.UI.CoreHost;
using Librova.UI.Desktop;
using Librova.UI.ImportJobs;
using Librova.UI.LibraryCatalog;
using Librova.UI.Logging;
using Librova.UI.Runtime;
using Librova.UI.Shell;
using Librova.UI.ViewModels;
using System.Diagnostics;
using Xunit;

namespace Librova.UI.Tests;

public sealed class ViewModelsTests
{
    [Fact]
    public async Task ImportJobsViewModel_StartsImportAndWaitsForTerminalResultThroughService()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.StartImportCommand.ExecuteAsyncForTests();

            Assert.Equal(42UL, viewModel.LastJobId);
            Assert.NotNull(viewModel.LastResult);
            Assert.Equal(ImportJobStatusModel.Completed, viewModel.LastResult!.Snapshot.Status);
            Assert.Equal("Completed", viewModel.StatusText);
            Assert.Equal("Processed 1 file. Imported 1, failed 0, skipped 0.", viewModel.ResultSummaryText);
            Assert.Equal("Processed 1 of 1 file (100%). Imported 1, failed 0, skipped 0.", viewModel.ProgressSummaryText);
            Assert.Equal("Watch for duplicates", viewModel.WarningsText);
            Assert.Equal("No error.", viewModel.ErrorText);
            Assert.True(service.WaitCalls > 0);
            Assert.True(service.TryGetSnapshotCalls > 0);
            Assert.False(viewModel.IsBusy);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_CommandRequiresPaths()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService());

        Assert.False(viewModel.StartImportCommand.CanExecute(null));

        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            viewModel.SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2");
            viewModel.WorkingDirectory = Path.Combine(sandboxRoot, "work");

            await WaitForConditionAsync(() => viewModel.StartImportCommand.CanExecute(null));
            Assert.True(viewModel.StartImportCommand.CanExecute(null));
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_SetsControlledErrorStateWhenImportFails()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        var viewModel = new ImportJobsViewModel(new FailingImportJobsService())
        {
            SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
            WorkingDirectory = Path.Combine(sandboxRoot, "work")
        };

        try
        {
            await viewModel.StartImportCommand.ExecuteAsyncForTests();

            Assert.Equal("transport failed", viewModel.StatusText);
            Assert.False(viewModel.IsBusy);
            Assert.Null(viewModel.LastResult);
            Assert.Equal("No completed job yet.", viewModel.ResultSummaryText);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_RefreshCommandLoadsCurrentSnapshot()
    {
        var service = new SnapshotOnlyImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.StartImportAsync();
            service.ReturnTerminalResult = false;
            await viewModel.RefreshCommand.ExecuteAsyncForTests();

            Assert.Equal("Still running", viewModel.StatusText);
            Assert.Equal("Processed 1 of 4 files (25%). Imported 1, failed 0, skipped 0.", viewModel.ProgressSummaryText);
            Assert.True(service.RefreshSnapshotCalls > 0);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_CancelCommandRequestsCancellationForActiveJob()
    {
        var service = new CancelAwareImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            var runTask = viewModel.StartImportAsync();
            await service.WaitForStartAsync();
            await service.WaitForPollingAsync();

            Assert.True(viewModel.CancelImportCommand.CanExecute(null));

            await viewModel.CancelImportCommand.ExecuteAsyncForTests();
            Assert.Contains("cancellation requested", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
            await runTask;

            Assert.True(service.CancelCalls > 0);
            Assert.Equal(ImportJobStatusModel.Cancelled, viewModel.LastResult?.Snapshot.Status);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_ShowsRollbackResidueInCancelledTerminalState()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(new CancellationResidueImportJobsService())
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.StartImportAsync();

            Assert.Equal(
                "Import was cancelled. Some managed files could not be removed during rollback.",
                viewModel.StatusText);
            Assert.Contains("left managed artifact", viewModel.WarningsText, StringComparison.Ordinal);
            Assert.Contains("Cancellation", viewModel.ErrorText, StringComparison.Ordinal);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_RemoveCommandClearsCurrentJobState()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.StartImportAsync();
            Assert.True(viewModel.RemoveJobCommand.CanExecute(null));

            await viewModel.RemoveJobCommand.ExecuteAsyncForTests();

            Assert.Equal(42UL, service.LastRemovedJobId);
            Assert.Null(viewModel.LastJobId);
            Assert.Null(viewModel.LastResult);
            Assert.Equal("No completed job yet.", viewModel.ResultSummaryText);
            Assert.Contains("was removed", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_BrowseCommandsPopulatePathsFromSelectionService()
    {
        var selectionService = new FakePathSelectionService
        {
            SelectedSourcePaths = [@"C:\Incoming\book.fb2", @"C:\Incoming\batch.epub"],
            SelectedWorkingDirectory = @"C:\Temp\Librova\Work"
        };
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService(), selectionService);

        await viewModel.BrowseSourceCommand.ExecuteAsyncForTests();
        await viewModel.BrowseWorkingDirectoryCommand.ExecuteAsyncForTests();

        Assert.Equal([@"C:\Incoming\book.fb2", @"C:\Incoming\batch.epub"], viewModel.SourcePaths);
        Assert.Equal(@"C:\Temp\Librova\Work", viewModel.WorkingDirectory);
    }

    [Fact]
    public async Task ImportJobsViewModel_SelectSourceAndImportCommand_StartsImportAfterPickingSource()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = CreateSupportedSourceFile(sandboxRoot, "selected.fb2");
            var viewModel = new ImportJobsViewModel(
                service,
                new FakePathSelectionService
                {
                    SelectedSourcePaths = [sourcePath]
                })
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.SelectSourceAndImportCommand.ExecuteAsyncForTests();

            Assert.Equal([sourcePath], viewModel.SourcePaths);
            Assert.Equal(42UL, viewModel.LastJobId);
            Assert.NotNull(viewModel.LastResult);
            Assert.Equal([sourcePath], service.LastStartRequest?.SourcePaths);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_SelectSourceAndImportCommand_StartsBatchImportAfterPickingMultipleFiles()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var firstSourcePath = CreateSupportedSourceFile(sandboxRoot, "selected-one.fb2");
            var secondSourcePath = CreateSupportedSourceFile(sandboxRoot, "selected-two.epub");
            var viewModel = new ImportJobsViewModel(
                service,
                new FakePathSelectionService
                {
                    SelectedSourcePaths = [firstSourcePath, secondSourcePath]
                })
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.SelectSourceAndImportCommand.ExecuteAsyncForTests();

            Assert.Equal([firstSourcePath, secondSourcePath], viewModel.SourcePaths);
            Assert.Equal(42UL, viewModel.LastJobId);
            Assert.Equal([firstSourcePath, secondSourcePath], service.LastStartRequest?.SourcePaths);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_UsesZeroFileSummaryWhenNoSupportedEntriesWereFound()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(new EmptyImportJobsService())
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "empty.zip"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.StartImportAsync();

            Assert.Equal("No supported files were found in the selected import sources.", viewModel.ResultSummaryText);
            Assert.Equal("No supported files were found in the selected import sources.", viewModel.ProgressSummaryText);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_SelectDirectoryAndImportCommand_StartsRecursiveDirectoryImport()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourceDirectory = Path.Combine(sandboxRoot, "incoming");
            Directory.CreateDirectory(sourceDirectory);
            File.WriteAllText(Path.Combine(sourceDirectory, "book.fb2"), "content");
            var viewModel = new ImportJobsViewModel(
                service,
                new FakePathSelectionService
                {
                    SelectedSourceDirectory = sourceDirectory
                })
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.SelectDirectoryAndImportCommand.ExecuteAsyncForTests();

            Assert.Equal([sourceDirectory], viewModel.SourcePaths);
            Assert.Equal(42UL, viewModel.LastJobId);
            Assert.Equal([sourceDirectory], service.LastStartRequest?.SourcePaths);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_ApplyDroppedSourcePathsAndStartAsync_HandlesStartFailuresWithoutThrowing()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = CreateSupportedSourceFile(sandboxRoot, "dropped.fb2");
            var viewModel = new ImportJobsViewModel(new FailingImportJobsService())
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await viewModel.ApplyDroppedSourcePathsAndStartAsync([sourcePath]);

            Assert.Equal([sourcePath], viewModel.SourcePaths);
            Assert.Equal("transport failed", viewModel.StatusText);
            Assert.False(viewModel.IsBusy);
            Assert.Null(viewModel.LastResult);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_ApplyDroppedSourcePathsAndStartAsync_IgnoresDroppedSourcesWhileImportIsRunning()
    {
        var service = new CancelAwareImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var originalSourcePath = CreateSupportedSourceFile(sandboxRoot, "running.fb2");
            var droppedSourcePath = CreateSupportedSourceFile(sandboxRoot, "dropped.epub");
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = originalSourcePath,
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            var runTask = viewModel.StartImportAsync();
            await service.WaitForStartAsync();
            await service.WaitForPollingAsync();

            Assert.True(viewModel.IsBusy);
            Assert.False(viewModel.CanAcceptNewSources);

            await viewModel.ApplyDroppedSourcePathsAndStartAsync([droppedSourcePath]);

            Assert.Equal([originalSourcePath], viewModel.SourcePaths);
            Assert.Equal(originalSourcePath, viewModel.SelectedSourcePathText);
            Assert.Equal(1, service.StartCalls);

            await viewModel.CancelCurrentAsync();
            await runTask;
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_ApplyDroppedSourcePathsAndStartAsync_DoesNotStartPreviousSelectionWhenDropIsIgnored()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var originalSourcePath = CreateSupportedSourceFile(sandboxRoot, "existing.fb2");
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = originalSourcePath,
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await WaitForConditionAsync(() => viewModel.StartImportCommand.CanExecute(null));
            await viewModel.ApplyDroppedSourcePathsAndStartAsync([]);

            Assert.Equal([originalSourcePath], viewModel.SourcePaths);
            Assert.Null(service.LastStartRequest);
            Assert.Null(viewModel.LastJobId);
            Assert.False(viewModel.IsBusy);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_AllowsMixedValidAndUnsupportedSelectedSourcesWhenAtLeastOneSourceIsImportable()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var validSourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2");
            var unsupportedSourcePath = Path.Combine(sandboxRoot, "notes.txt");
            await File.WriteAllTextAsync(unsupportedSourcePath, "ignored");

            var viewModel = new ImportJobsViewModel(service)
            {
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            viewModel.ApplyDroppedSourcePaths([validSourcePath, unsupportedSourcePath]);

            await WaitForConditionAsync(() => viewModel.StartImportCommand.CanExecute(null));
            Assert.True(viewModel.StartImportCommand.CanExecute(null));
            Assert.Equal(string.Empty, viewModel.SourceValidationMessage);

            await viewModel.StartImportAsync();

            Assert.Equal([validSourcePath, unsupportedSourcePath], service.LastStartRequest?.SourcePaths);
            Assert.Equal(42UL, viewModel.LastJobId);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_StartImportPassesProbableDuplicateOverrideFlag()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work"),
                AllowProbableDuplicates = true
            };

            await viewModel.StartImportCommand.ExecuteAsyncForTests();

            Assert.NotNull(service.LastStartRequest);
            Assert.True(service.LastStartRequest!.AllowProbableDuplicates);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_StartImportPassesForcedEpubConversionFlag()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work"),
                HasConfiguredConverter = true,
                ForceEpubConversionOnImport = true
            };

            await viewModel.StartImportCommand.ExecuteAsyncForTests();

            Assert.NotNull(service.LastStartRequest);
            Assert.True(service.LastStartRequest!.ForceEpubConversionOnImport);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public void ImportJobsViewModel_HidesForcedEpubConversionOptionWithoutConfiguredConverter()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService());

        Assert.False(viewModel.ShowForceEpubConversionOption);

        viewModel.HasConfiguredConverter = true;
        viewModel.ForceEpubConversionOnImport = true;

        Assert.True(viewModel.ShowForceEpubConversionOption);
        Assert.True(viewModel.ForceEpubConversionOnImport);

        viewModel.HasConfiguredConverter = false;

        Assert.False(viewModel.ShowForceEpubConversionOption);
        Assert.False(viewModel.ForceEpubConversionOnImport);
    }

    [Fact]
    public void ImportJobsViewModel_DisablesStartForMissingSourceFile()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
        {
            SourcePath = Path.Combine(Path.GetTempPath(), $"{Guid.NewGuid():N}.fb2"),
            WorkingDirectory = @"C:\Work"
        };

        Assert.False(viewModel.StartImportCommand.CanExecute(null));
        Assert.Equal("A selected source does not exist.", viewModel.SourceValidationMessage);
    }

    [Fact]
    public async Task ImportJobsViewModel_EnablesStartForExistingSupportedSourceFile()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = Path.Combine(sandboxRoot, "book.fb2");
            await File.WriteAllTextAsync(sourcePath, "<fb2 />");

            var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
            {
                SourcePath = sourcePath,
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await WaitForConditionAsync(() => viewModel.StartImportCommand.CanExecute(null));
            Assert.True(viewModel.StartImportCommand.CanExecute(null));
            Assert.Equal(string.Empty, viewModel.SourceValidationMessage);
            Assert.Equal(string.Empty, viewModel.WorkingDirectoryValidationMessage);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_DisablesStartForUnsupportedSourceExtension()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = Path.Combine(sandboxRoot, "book.txt");
            File.WriteAllText(sourcePath, "plain text");

            var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
            {
                SourcePath = sourcePath,
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            await WaitForConditionAsync(() => !viewModel.StartImportCommand.CanExecute(null) && viewModel.SourceValidationMessage.Length > 0);
            Assert.False(viewModel.StartImportCommand.CanExecute(null));
            Assert.Equal("Supported source types are .fb2, .epub, and .zip, or a directory containing them.", viewModel.SourceValidationMessage);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_StartImportAsync_ThrowsWhenCoreValidationRejectsSelectedSources()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = Path.Combine(sandboxRoot, "book.txt");
            File.WriteAllText(sourcePath, "plain text");

            var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
            {
                SourcePath = sourcePath,
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            var error = await Assert.ThrowsAsync<InvalidOperationException>(() => viewModel.StartImportAsync());
            Assert.Equal("Supported source types are .fb2, .epub, and .zip, or a directory containing them.", error.Message);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public void ImportJobsViewModel_DisablesStartWhenWorkingDirectoryPointsToFile()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var sourcePath = Path.Combine(sandboxRoot, "book.epub");
            File.WriteAllText(sourcePath, "epub");
            var filePath = Path.Combine(sandboxRoot, "not-a-directory.tmp");
            File.WriteAllText(filePath, "tmp");

            var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
            {
                SourcePath = sourcePath,
                WorkingDirectory = filePath
            };

            Assert.False(viewModel.StartImportCommand.CanExecute(null));
            Assert.Equal("Working directory must not point to a file.", viewModel.WorkingDirectoryValidationMessage);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public void ImportJobsViewModel_ApplyDroppedSourcePathUpdatesSourceField()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService());

        viewModel.ApplyDroppedSourcePath(@"C:\Incoming\dropped.epub");

        Assert.Equal(@"C:\Incoming\dropped.epub", viewModel.SourcePath);
    }

    [Fact]
    public void ImportJobsViewModel_ApplyDroppedSourcePathIgnoresBlankValues()
    {
        var viewModel = new ImportJobsViewModel(new FakeImportJobsService())
        {
            SourcePath = @"C:\Incoming\existing.fb2"
        };

        viewModel.ApplyDroppedSourcePath("   ");

        Assert.Equal(@"C:\Incoming\existing.fb2", viewModel.SourcePath);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_LoadsBooksFromCatalogService()
    {
        var viewModel = new LibraryBrowserViewModel(new FakeLibraryCatalogService());
        viewModel.SearchText = "road";

        await viewModel.RefreshAsync();

        Assert.True(viewModel.HasBooks);
        Assert.Equal("Loaded 1 book(s).", viewModel.StatusText);
        Assert.Single(viewModel.Books);
        Assert.Equal("Roadside Picnic", viewModel.Books[0].Title);
        Assert.Null(viewModel.SelectedBook);
        Assert.Equal("1 book", viewModel.BookCountText);
        Assert.Equal("Library: 1 book, 0.00 MB", viewModel.LibraryStatisticsText);
        Assert.True(viewModel.Books[0].ShowCoverPlaceholder);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_LoadsCoverImageThroughLoaderAgainstLibraryRoot()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        var coversRoot = Path.Combine(sandboxRoot, "Covers");
        Directory.CreateDirectory(coversRoot);

        try
        {
            var coverPath = Path.Combine(coversRoot, "0000000001.png");
            await File.WriteAllTextAsync(coverPath, "cover-path-marker");

            var service = new CoverAwareLibraryCatalogService();
            var loader = new RecordingCoverImageLoader();
            var viewModel = new LibraryBrowserViewModel(service, libraryRoot: sandboxRoot, coverImageLoader: loader);

            await viewModel.RefreshAsync();

            Assert.Single(loader.LoadedPaths);
            Assert.Equal(coverPath, loader.LoadedPaths[0]);

            await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

            Assert.Equal(2, loader.LoadedPaths.Count);
            Assert.Equal(coverPath, loader.LoadedPaths[1]);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public void BookListItemModel_KeepsCatalogDataSeparateFromPresentationState()
    {
        var model = new BookListItemModel
        {
            Data = new BookListItemDataModel
            {
                BookId = 7,
                Title = "Roadside Picnic",
                Authors = ["Arkady Strugatsky"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000007/book.epub"
            }
        };

        model.CoverPlaceholderText = "R";
        model.IsSelected = true;
        model.CardPresentation.CardBorderThickness = new Thickness(2);

        Assert.Equal(7, model.BookId);
        Assert.Equal("Roadside Picnic", model.Data.Title);
        Assert.True(model.IsSelected);
        Assert.Equal("R", model.CoverPlaceholderText);
        Assert.Equal(new Thickness(2), model.CardBorderThickness);
    }

    [Fact]
    public void BookDetailsModel_KeepsCatalogDataSeparateFromPresentationState()
    {
        var model = new BookDetailsModel
        {
            Data = new BookDetailsDataModel
            {
                BookId = 11,
                Title = "Definitely Maybe",
                Authors = ["Arkady Strugatsky", "Boris Strugatsky"],
                Language = "en",
                Format = BookFormatModel.Fb2,
                ManagedPath = "Books/0000000011/book.fb2",
                Storage = new BookStorageInfoModel
                {
                    ManagedRelativePath = "Books/0000000011/book.fb2",
                    HasContentHash = true
                }
            }
        };

        model.CoverPlaceholderText = "D";

        Assert.Equal(11, model.BookId);
        Assert.True(model.HasContentHash);
        Assert.Equal("D", model.CoverPlaceholderText);
        Assert.Equal("D", model.CoverPresentation.CoverPlaceholderText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ReportsEmptySearchResult()
    {
        var viewModel = new LibraryBrowserViewModel(new EmptyLibraryCatalogService());

        await viewModel.RefreshCommand.ExecuteAsyncForTests();

        Assert.False(viewModel.HasBooks);
        Assert.Equal("No books found for the current filter.", viewModel.StatusText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_UsesGradientPlaceholderBackgroundWhenCoverIsMissing()
    {
        var viewModel = new LibraryBrowserViewModel(new FakeLibraryCatalogService());

        await viewModel.RefreshAsync();

        Assert.True(viewModel.Books[0].ShowCoverPlaceholder);
        Assert.IsType<LinearGradientBrush>(viewModel.Books[0].CoverBackgroundBrush);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_UsesNeutralBackgroundWhenRealCoverIsLoaded()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        var coversRoot = Path.Combine(sandboxRoot, "Covers");
        Directory.CreateDirectory(coversRoot);

        try
        {
            await File.WriteAllTextAsync(Path.Combine(coversRoot, "0000000001.png"), "stub-cover-file");

            var loader = new StubCoverImageLoader();
            var viewModel = new LibraryBrowserViewModel(
                new CoverAwareLibraryCatalogService(),
                libraryRoot: sandboxRoot,
                coverImageLoader: loader);

            await viewModel.RefreshAsync();

            Assert.False(viewModel.Books[0].ShowCoverPlaceholder);
            var gridCoverBackground = Assert.IsAssignableFrom<ISolidColorBrush>(viewModel.Books[0].CoverBackgroundBrush);
            Assert.Equal(Color.Parse("#0A0700"), gridCoverBackground.Color);

            await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

            Assert.False(viewModel.ShowSelectedBookCoverPlaceholder);
            var detailsCoverBackground = Assert.IsAssignableFrom<ISolidColorBrush>(viewModel.SelectedBookCoverBackgroundBrush);
            Assert.Equal(Color.Parse("#0A0700"), detailsCoverBackground.Color);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task LibraryBrowserViewModel_RejectsAbsoluteCoverPathOutsideLibraryRoot()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        var outsidePath = Path.Combine(Path.GetTempPath(), $"outside-cover-{Guid.NewGuid():N}.png");

        try
        {
            await File.WriteAllTextAsync(outsidePath, "cover-outside-root");

            var service = new FixedCoverLibraryCatalogService(outsidePath);
            var loader = new RecordingCoverImageLoader();
            var viewModel = new LibraryBrowserViewModel(service, libraryRoot: sandboxRoot, coverImageLoader: loader);

            await viewModel.RefreshAsync();

            Assert.Empty(loader.LoadedPaths);
        }
        finally
        {
            try { File.Delete(outsidePath); } catch (IOException) { } catch (UnauthorizedAccessException) { }
            try { Directory.Delete(sandboxRoot, true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
        }
    }

    [Fact]
    public async Task LibraryBrowserViewModel_RejectsPathTraversalCoverPath()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var service = new FixedCoverLibraryCatalogService("../../outside-cover.png");
            var loader = new RecordingCoverImageLoader();
            var viewModel = new LibraryBrowserViewModel(service, libraryRoot: sandboxRoot, coverImageLoader: loader);

            await viewModel.RefreshAsync();

            Assert.Empty(loader.LoadedPaths);
        }
        finally
        {
            try { Directory.Delete(sandboxRoot, true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
        }
    }

    [Fact]
    public async Task LibraryBrowserViewModel_RejectsAbsoluteCoverPathWithDotDotEscapingRoot()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        var outsideFileName = $"escape-cover-{Guid.NewGuid():N}.png";
        var resolvedOutsidePath = Path.Combine(Path.GetDirectoryName(sandboxRoot)!, outsideFileName);
        var dotDotAbsolutePath = Path.Combine(sandboxRoot, "..", outsideFileName);

        try
        {
            await File.WriteAllTextAsync(resolvedOutsidePath, "escape-cover-data");

            var service = new FixedCoverLibraryCatalogService(dotDotAbsolutePath);
            var loader = new RecordingCoverImageLoader();
            var viewModel = new LibraryBrowserViewModel(service, libraryRoot: sandboxRoot, coverImageLoader: loader);

            await viewModel.RefreshAsync();

            Assert.Empty(loader.LoadedPaths);
        }
        finally
        {
            try { File.Delete(resolvedOutsidePath); } catch (IOException) { } catch (UnauthorizedAccessException) { }
            try { Directory.Delete(sandboxRoot, true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
        }
    }

    [Fact]
    public async Task LibraryBrowserViewModel_AcceptsAbsoluteCoverPathWithinLibraryRoot()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        var coversDir = Path.Combine(sandboxRoot, "Covers");
        Directory.CreateDirectory(coversDir);

        var absoluteCoverPath = Path.Combine(coversDir, $"{Guid.NewGuid():N}.png");

        try
        {
            await File.WriteAllTextAsync(absoluteCoverPath, "cover-data");

            var service = new FixedCoverLibraryCatalogService(absoluteCoverPath);
            var loader = new RecordingCoverImageLoader();
            var viewModel = new LibraryBrowserViewModel(service, libraryRoot: sandboxRoot, coverImageLoader: loader);

            await viewModel.RefreshAsync();

            Assert.Single(loader.LoadedPaths);
            Assert.Equal(absoluteCoverPath, loader.LoadedPaths[0]);
        }
        finally
        {
            try { Directory.Delete(sandboxRoot, true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
        }
    }

    [Fact]
    public async Task LibraryBrowserViewModel_RejectsCoverPathEscapingLibraryRootThroughJunction()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        var outsideRoot = Path.Combine(Path.GetTempPath(), "librova-ui-outside", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        Directory.CreateDirectory(outsideRoot);

        var junctionPath = Path.Combine(sandboxRoot, "CoversLink");
        var outsideCoverPath = Path.Combine(outsideRoot, "escaped-cover.png");

        try
        {
            await File.WriteAllTextAsync(outsideCoverPath, "cover-data");
            CreateDirectoryJunction(junctionPath, outsideRoot);

            var service = new FixedCoverLibraryCatalogService(Path.Combine("CoversLink", "escaped-cover.png"));
            var loader = new RecordingCoverImageLoader();
            var viewModel = new LibraryBrowserViewModel(service, libraryRoot: sandboxRoot, coverImageLoader: loader);

            await viewModel.RefreshAsync();

            Assert.Empty(loader.LoadedPaths);
        }
        finally
        {
            try { Directory.Delete(junctionPath, false); } catch (IOException) { } catch (UnauthorizedAccessException) { }
            try { Directory.Delete(sandboxRoot, true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
            try { Directory.Delete(outsideRoot, true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
        }
    }

    [Fact]
    public async Task LibraryBrowserViewModel_PassesSelectedLanguagesAndGenresIntoCatalogRequest()
    {
        var service = new RecordingLibraryCatalogService
        {
            AvailableLanguages = ["en", "ru"],
            AvailableGenres = ["sci-fi", "adventure"]
        };
        var viewModel = new LibraryBrowserViewModel(service)
        {
            SearchText = "road",
            PageSize = 10
        };

        await viewModel.RefreshAsync();

        viewModel.LanguageFacets.Single(f => f.Value == "en").IsSelected = true;
        viewModel.GenreFacets.Single(f => f.Value == "sci-fi").IsSelected = true;

        await viewModel.RefreshAsync();

        Assert.NotNull(service.LastRequest);
        Assert.Equal("road", service.LastRequest!.Text);
        Assert.Equal(["en"], service.LastRequest.Languages);
        Assert.Equal(["sci-fi"], service.LastRequest.Genres);
        Assert.Null(service.LastRequest.Author);
        Assert.Null(service.LastRequest.Format);
        Assert.Equal(BookSortModel.Title, service.LastRequest.SortBy);
        Assert.Equal(0UL, service.LastRequest.Offset);
        Assert.Equal(10UL, service.LastRequest.Limit);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_SearchRefreshesAsTextChangesAndRestoresBooksAfterClearing()
    {
        var service = new QueryFilteringLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        Assert.Equal(2, viewModel.Books.Count);

        viewModel.SearchText = "road";
        await WaitForConditionAsync(() => service.ListCalls >= 2 && viewModel.Books.Count == 1);

        Assert.Single(viewModel.Books);
        Assert.Equal("Roadside Picnic", viewModel.Books[0].Title);
        Assert.Equal("1 book", viewModel.BookCountText);

        viewModel.SearchText = "missing";
        await WaitForConditionAsync(() => service.ListCalls >= 3 && viewModel.Books.Count == 0);

        Assert.False(viewModel.HasBooks);
        Assert.Equal("Nothing found", viewModel.EmptyStateTitle);

        viewModel.SearchText = string.Empty;
        await WaitForConditionAsync(() => service.ListCalls >= 4 && viewModel.Books.Count == 2);

        Assert.Equal(2, viewModel.Books.Count);
        Assert.Equal("2 books", viewModel.BookCountText);
        Assert.Equal("Library: 2 books, 0.00 MB", viewModel.LibraryStatisticsText);
        Assert.Equal("Loaded 2 book(s).", viewModel.StatusText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_DisposeCancelsPendingDebouncedRefresh()
    {
        var service = new RecordingLibraryCatalogService();
        using var viewModel = new LibraryBrowserViewModel(service);

        viewModel.SearchText = "road";
        viewModel.Dispose();

        await Task.Delay(400);

        Assert.Null(service.LastRequest);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_KeepsOtherKnownLanguagesVisibleWhenLanguageFilterIsApplied()
    {
        var service = new QueryFilteringLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        Assert.Contains("en", viewModel.LanguageFacets.Select(f => f.Value));
        Assert.Contains("ru", viewModel.LanguageFacets.Select(f => f.Value));

        viewModel.LanguageFacets.Single(f => f.Value == "en").IsSelected = true;
        await WaitForConditionAsync(() => service.ListCalls >= 2 && viewModel.Books.All(book => book.Language == "en"));

        Assert.Contains("en", viewModel.LanguageFacets.Select(f => f.Value));
        Assert.Contains("ru", viewModel.LanguageFacets.Select(f => f.Value));
    }

    [Fact]
    public async Task LibraryBrowserViewModel_KeepsOtherKnownGenresVisibleWhenGenreFilterIsApplied()
    {
        var service = new QueryFilteringLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        Assert.Contains("adventure", viewModel.GenreFacets.Select(f => f.Value));
        Assert.Contains("sci-fi", viewModel.GenreFacets.Select(f => f.Value));

        viewModel.GenreFacets.Single(f => f.Value == "sci-fi").IsSelected = true;
        await WaitForConditionAsync(() => service.ListCalls >= 2 && viewModel.Books.All(book => book.Tags.Contains("sci-fi", StringComparer.OrdinalIgnoreCase)));

        Assert.Contains("adventure", viewModel.GenreFacets.Select(f => f.Value));
        Assert.Contains("sci-fi", viewModel.GenreFacets.Select(f => f.Value));
    }

    [Fact]
    public async Task LibraryBrowserViewModel_KeepsSelectedFacetVisibleWhenBackendOmitsItFromAvailableValues()
    {
        var service = new OmittingSelectedFacetLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        viewModel.LanguageFacets.Single(f => f.Value == "ru").IsSelected = true;
        await WaitForConditionAsync(() => service.ListCalls >= 2 && viewModel.Books.All(book => book.Language == "ru"));

        Assert.Contains("ru", viewModel.LanguageFacets.Select(f => f.Value));
        Assert.True(viewModel.LanguageFacets.Single(f => f.Value == "ru").IsSelected);
        Assert.Equal(["ru"], service.LastRequest?.Languages);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ShowsLanguagesBeyondTheFirstLoadedBatch()
    {
        var service = new PagedLanguageLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            PageSize = 2
        };

        await viewModel.RefreshAsync();

        Assert.Equal(["Alpha", "Beta"], viewModel.Books.Select(book => book.Title).ToArray());
        Assert.Contains("en", viewModel.LanguageFacets.Select(f => f.Value));
        Assert.Contains("ru", viewModel.LanguageFacets.Select(f => f.Value));
        Assert.Equal("3 books", viewModel.BookCountText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_RemovesLanguageWhenTheLastBookUsingItIsDeleted()
    {
        var service = new LanguageDeletionLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books.Single(book => book.Language == "ru"));

        await viewModel.MoveSelectedBookToTrashAsync();

        Assert.DoesNotContain("ru", viewModel.LanguageFacets.Select(f => f.Value));
        Assert.Contains("en", viewModel.LanguageFacets.Select(f => f.Value));
        Assert.Equal("1 book", viewModel.BookCountText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ReportsLoadMoreFailureWithoutThrowing()
    {
        var service = new FailingLoadMoreLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            PageSize = 2
        };

        await viewModel.RefreshAsync();
        await viewModel.LoadMoreAsync();

        Assert.Equal(2, viewModel.Books.Count);
        Assert.Equal("load more failed", viewModel.StatusText);
        Assert.True(viewModel.HasMoreResults);
        Assert.False(viewModel.IsLoadingMore);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_RefreshesLoadedRangeInSingleRequestAfterDeletingBook()
    {
        var service = new DeleteAfterDeepScrollLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            PageSize = 2
        };

        await viewModel.RefreshAsync();
        await viewModel.LoadMoreAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[2]);

        await viewModel.MoveSelectedBookToTrashAsync();

        Assert.Equal(3, service.ListCalls);
        Assert.Equal(4UL, service.Requests[^1].Limit);
        Assert.Equal(0UL, service.Requests[^1].Offset);
        Assert.Equal(["Alpha", "Beta", "Delta"], viewModel.Books.Select(book => book.Title).ToArray());
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ShowsAggregateLibraryStatisticsSeparatelyFromVisiblePage()
    {
        var service = new FakeLibraryCatalogService
        {
            Statistics = new LibraryStatisticsModel
            {
                BookCount = 42,
                TotalLibrarySizeBytes = 5UL * 1024UL * 1024UL
            }
        };
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();

        Assert.Single(viewModel.Books);
        Assert.Equal("1 book", viewModel.BookCountText);
        Assert.Equal("Library: 42 books, 5.00 MB", viewModel.LibraryStatisticsText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_StillLoadsBooksWhenLibraryStatisticsFail()
    {
        var viewModel = new LibraryBrowserViewModel(new StatisticsFailureLibraryCatalogService());

        await viewModel.RefreshAsync();

        Assert.True(viewModel.HasBooks);
        Assert.Single(viewModel.Books);
        Assert.Equal("Roadside Picnic", viewModel.Books[0].Title);
        Assert.Equal("Library summary unavailable.", viewModel.LibraryStatisticsText);
        Assert.Equal("Loaded 1 book(s). Library summary unavailable.", viewModel.StatusText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_LogsStatisticsFailuresWhenFallingBackToUnavailableSummary()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        var logPath = Path.Combine(sandboxRoot, "ui.log");

        try
        {
            UiLogging.ReinitializeForTests(logPath);
            var viewModel = new LibraryBrowserViewModel(new StatisticsFailureLibraryCatalogService());

            await viewModel.RefreshAsync();
            UiLogging.Shutdown();

            var logText = File.ReadAllText(logPath);
            Assert.Contains("ListBooks response did not include library statistics", logText, StringComparison.Ordinal);
        }
        finally
        {
            UiLogging.Shutdown();
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task LibraryBrowserViewModel_LoadsAdditionalPagesIntoExistingCollection()
    {
        var service = new PagingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            PageSize = 2
        };

        await viewModel.RefreshAsync();
        Assert.Equal(2, viewModel.Books.Count);
        Assert.True(viewModel.HasMoreResults);
        Assert.Null(viewModel.SelectedBook);

        await viewModel.LoadMoreAsync();
        Assert.Equal(3, viewModel.Books.Count);
        Assert.Equal(["Alpha", "Beta", "Gamma"], viewModel.Books.Select(book => book.Title).ToArray());
        Assert.Null(viewModel.SelectedBook);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_RaisesHasMoreResultsOnlyWhenPagingStateChanges()
    {
        var service = new PagingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            PageSize = 2
        };
        var notifications = new List<bool>();
        viewModel.PropertyChanged += (_, eventArgs) =>
        {
            if (eventArgs.PropertyName == nameof(LibraryBrowserViewModel.HasMoreResults))
            {
                notifications.Add(viewModel.HasMoreResults);
            }
        };

        await viewModel.RefreshAsync();
        await viewModel.RefreshAsync();
        await viewModel.LoadMoreAsync();

        Assert.Equal([true, false], notifications);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_DoesNotExposePhantomNextPageForExactlyFullPage()
    {
        var service = new ExactPageLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service)
        {
            PageSize = 2
        };

        await viewModel.RefreshAsync();

        Assert.False(viewModel.HasMoreResults);
        Assert.Equal(2UL, service.LastRequest!.Limit);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_LoadsExtendedDetailsForSelectedBook()
    {
        var service = new FakeLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        await viewModel.LoadDetailsCommand.ExecuteAsyncForTests();

        Assert.NotNull(viewModel.SelectedBookDetails);
        Assert.Contains(viewModel.SelectedBookMetadataPairs, pair => pair.Label == "Publisher" && pair.Value == "Macmillan");
        Assert.Contains(viewModel.SelectedBookMetadataPairs, pair => pair.Label == "Series" && pair.Value.StartsWith("Noon Universe #", StringComparison.Ordinal));
        Assert.Contains(viewModel.SelectedBookMetadataPairs, pair => pair.Label == "Genres" && pair.Value == "sci-fi, adventure");
        Assert.Contains(viewModel.SelectedBookMetadataPairs, pair => pair.Label == "Size" && pair.Value == "0.00 MB");
        Assert.Equal("Aliens land only in one city.", viewModel.SelectedBookAnnotationText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ExportsSelectedBookThroughPathSelectionService()
    {
        var service = new FakeLibraryCatalogService();
        var selectionService = new FakePathSelectionService
        {
            SelectedExportPath = @"C:\Exports\alpha.epub"
        };
        var viewModel = new LibraryBrowserViewModel(service, selectionService);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        await viewModel.ExportSelectedBookAsync();

        Assert.Equal(viewModel.SelectedBook!.BookId, service.LastExportBookId);
        Assert.Equal(@"C:\Exports\alpha.epub", service.LastExportPath);
        Assert.Null(service.LastExportFormat);
        Assert.Equal(TimeSpan.FromSeconds(10), service.LastExportTimeout);
        Assert.Equal("Roadside Picnic - Arkady Strugatsky.epub", selectionService.LastSuggestedExportFileName);
        Assert.Contains("Exported", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_UsesSanitizedTitleAndAuthorForSuggestedExportFileName()
    {
        var service = new InvalidMetadataLibraryCatalogService();
        var selectionService = new FakePathSelectionService
        {
            SelectedExportPath = @"C:\Exports\sanitized.fb2"
        };
        var viewModel = new LibraryBrowserViewModel(service, selectionService);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
        await viewModel.ExportSelectedBookAsync();

        Assert.Equal("Roadside Picnic Director's Cut - Arkady Boris Strugatsky.fb2", selectionService.LastSuggestedExportFileName);
        Assert.Equal(@"C:\Exports\sanitized.fb2", service.LastExportPath);
        Assert.Null(service.LastExportFormat);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ExportsFb2AsEpubWhenConverterIsConfigured()
    {
        var service = new InvalidMetadataLibraryCatalogService();
        var selectionService = new FakePathSelectionService
        {
            SelectedExportPath = @"C:\Exports\converted.epub"
        };
        var viewModel = new LibraryBrowserViewModel(service, selectionService, hasConfiguredConverter: true);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
        Assert.True(viewModel.ShowExportAsEpubAction);
        Assert.False(viewModel.ShowExportAsEpubHint);
        Assert.True(viewModel.ExportSelectedBookAsEpubCommand.CanExecute(null));

        await viewModel.ExportSelectedBookAsEpubAsync();

        Assert.Equal(BookFormatModel.Epub, service.LastExportFormat);
        Assert.Equal(@"C:\Exports\converted.epub", service.LastExportPath);
        Assert.Equal(TimeSpan.FromMinutes(2), service.LastExportTimeout);
        Assert.Equal("Roadside Picnic Director's Cut - Arkady Boris Strugatsky.epub", selectionService.LastSuggestedExportFileName);
        Assert.Contains("as EPUB", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_SuggestedExportFileNameUsesFormatNotManagedPathExtension()
    {
        // Regression: managed FB2 files are stored as .fb2.gz on disk; the suggested export
        // filename must use the book's logical format (.fb2), not the on-disk storage extension (.gz).
        var service = new CompressedFb2LibraryCatalogService();
        var selectionService = new FakePathSelectionService
        {
            SelectedExportPath = @"C:\Exports\book.fb2"
        };
        var viewModel = new LibraryBrowserViewModel(service, selectionService);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
        await viewModel.ExportSelectedBookAsync();

        Assert.Equal("Roadside Picnic - Arkady Strugatsky.fb2", selectionService.LastSuggestedExportFileName);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_ShowsExportAsEpubHintWhenConverterIsNotConfigured()
    {
        var viewModel = new LibraryBrowserViewModel(new InvalidMetadataLibraryCatalogService());

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        Assert.False(viewModel.ShowExportAsEpubAction);
        Assert.True(viewModel.ShowExportAsEpubHint);
        Assert.Contains("Settings", viewModel.ExportAsEpubHintText, StringComparison.Ordinal);
        Assert.False(viewModel.ExportSelectedBookAsEpubCommand.CanExecute(null));
    }

    [Fact]
    public async Task LibraryBrowserViewModel_SetsIsExportBusyDuringExportAndClearsAfterCompletion()
    {
        var exportGate = new TaskCompletionSource();
        var service = new GatedExportLibraryCatalogService(exportGate.Task);
        var selectionService = new FakePathSelectionService
        {
            SelectedExportPath = @"C:\Exports\busy.epub"
        };
        var viewModel = new LibraryBrowserViewModel(service, selectionService, hasConfiguredConverter: true);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        Assert.False(viewModel.IsExportBusy);

        // Start export without awaiting — the export will block at the gate inside the service.
        var exportTask = viewModel.ExportSelectedBookAsEpubAsync();

        // Yield so the async method runs up to the first real await (the gated service call).
        // At that point IsBusy and IsExportBusy must already be true.
        await Task.Yield();

        Assert.True(viewModel.IsExportBusy);
        Assert.True(viewModel.IsBusy);
        Assert.Contains("Exporting", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);

        // Release the gate to let the export complete.
        exportGate.SetResult();
        await exportTask;

        Assert.False(viewModel.IsExportBusy);
        Assert.False(viewModel.IsBusy);
        Assert.Contains("Exported", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_MovesSelectedBookToTrashThroughCatalogService()
    {
        var service = new FakeLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
        var selectedBookId = viewModel.SelectedBook!.BookId;

        await viewModel.MoveSelectedBookToTrashAsync();

        Assert.Equal(selectedBookId, service.LastTrashedBookId);
        Assert.Contains("Recycle Bin", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_WarnsWhenDeleteLeavesOrphanedFiles()
    {
        var service = new OrphanedDeleteLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        await viewModel.MoveSelectedBookToTrashAsync();

        Assert.Contains("on disk", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
        Assert.Contains("Recycle Bin", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_DoesNotClaimManagedTrashWhenDeleteLeavesOnlyOrphanedFiles()
    {
        var service = new ManagedTrashOrphanedDeleteLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        await viewModel.MoveSelectedBookToTrashAsync();

        Assert.Contains("on disk", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
        Assert.StartsWith("Removed", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
        Assert.DoesNotContain("library Trash", viewModel.StatusText, StringComparison.OrdinalIgnoreCase);
    }

    [Fact]
    public void LibraryCoverPresentationService_SelectPaletteIndex_HandlesIntMinValue()
    {
        var index = LibraryCoverPresentationService.SelectPaletteIndex(int.MinValue, 5);

        Assert.InRange(index, 0, 4);
    }

    [Fact]
    public void LibraryCoverPresentationService_LogsCoverLoadFailuresWithPathContext()
    {
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);
        var libraryRoot = Path.Combine(sandboxRoot, "Library");
        Directory.CreateDirectory(Path.Combine(libraryRoot, "Covers"));
        var logPath = Path.Combine(sandboxRoot, "ui.log");
        var coverPath = "Covers/failing-cover.png";
        File.WriteAllText(Path.Combine(libraryRoot, coverPath), "stub-cover");

        try
        {
            UiLogging.ReinitializeForTests(logPath);
            var service = new LibraryCoverPresentationService(
                libraryRoot,
                new ThrowingCoverImageLoader(new InvalidOperationException("decoder exploded")));

            var item = service.Prepare(new BookListItemModel
            {
                BookId = 42,
                Title = "Covered Book",
                Authors = ["Author"],
                CoverPath = coverPath
            });

            UiLogging.Shutdown();

            Assert.Null(item.ResolvedCoverImage);
            var logText = File.ReadAllText(logPath);
            Assert.Contains("Failed to load managed cover image.", logText, StringComparison.Ordinal);
            Assert.Contains(coverPath, logText, StringComparison.Ordinal);
            Assert.Contains(libraryRoot, logText, StringComparison.Ordinal);
            Assert.Contains("decoder exploded", logText, StringComparison.Ordinal);
        }
        finally
        {
            UiLogging.Shutdown();
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public void LibraryBrowserViewModel_ClampsInvalidPageSize()
    {
        var viewModel = new LibraryBrowserViewModel(new EmptyLibraryCatalogService())
        {
            PageSize = 0
        };

        Assert.Equal(1, viewModel.PageSize);

        viewModel.PageSize = -10;

        Assert.Equal(1, viewModel.PageSize);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_TogglesSelectionAndLoadsDetails()
    {
        var service = new FakeLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        var book = viewModel.Books[0];

        await viewModel.ToggleSelectedBookAsync(book);

        Assert.Equal(book.BookId, viewModel.SelectedBook?.BookId);
        Assert.True(viewModel.ShowDetailsPanel);
        Assert.True(book.IsSelected);
        Assert.Equal("Roadside Picnic", viewModel.SelectedBookTitle);
        Assert.Equal("Arkady Strugatsky", viewModel.SelectedBookAuthorText);
        Assert.NotNull(viewModel.SelectedBookDetails);

        await viewModel.ToggleSelectedBookAsync(book);

        Assert.Null(viewModel.SelectedBook);
        Assert.False(viewModel.ShowDetailsPanel);
        Assert.False(book.IsSelected);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_RefreshPreservesSelectionWithoutReloadingDetails()
    {
        var service = new RefreshPreservingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);

        Assert.Equal(1, service.GetBookDetailsCalls);
        Assert.NotNull(viewModel.SelectedBookDetails);

        await viewModel.RefreshAsync();

        Assert.Equal(1, service.GetBookDetailsCalls);
        Assert.Equal(1L, viewModel.SelectedBook?.BookId);
        Assert.Null(viewModel.SelectedBookDetails);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_UsesLegacyStatusTextWhenDetailsRpcReturnsStructuredNotFound()
    {
        var viewModel = new LibraryBrowserViewModel(new NotFoundDetailsLibraryCatalogService());

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
        await viewModel.LoadDetailsCommand.ExecuteAsyncForTests();

        Assert.Null(viewModel.SelectedBookDetails);
        Assert.Equal("Book details were not found.", viewModel.StatusText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_UsesLegacyStatusTextWhenExportRpcReturnsStructuredNotFound()
    {
        var selectionService = new FakePathSelectionService
        {
            SelectedExportPath = @"C:\Exports\missing.fb2"
        };
        var viewModel = new LibraryBrowserViewModel(new NotFoundExportLibraryCatalogService(), selectionService);

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
        await viewModel.ExportSelectedBookAsync();

        Assert.Equal("Selected book could not be exported.", viewModel.StatusText);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_UsesLegacyStatusTextWhenDeleteRpcReturnsStructuredNotFound()
    {
        var viewModel = new LibraryBrowserViewModel(new NotFoundDeleteLibraryCatalogService());

        await viewModel.RefreshAsync();
        await viewModel.ToggleSelectedBookAsync(viewModel.Books[0]);
        await viewModel.MoveSelectedBookToTrashAsync();

        Assert.Equal("Selected book could not be moved to Recycle Bin.", viewModel.StatusText);
    }

    [Fact]
    public async Task ImportJobsViewModel_EmitsSuccessEventForCompletedImport()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            var invoked = 0;
            viewModel.ImportCompletedSuccessfully += _ =>
            {
                invoked++;
                return Task.CompletedTask;
            };

            await viewModel.StartImportAsync();

            Assert.Equal(1, invoked);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public async Task ImportJobsViewModel_AwaitsAllSuccessSubscribersSequentially()
    {
        var service = new FakeImportJobsService();
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var viewModel = new ImportJobsViewModel(service)
            {
                SourcePath = CreateSupportedSourceFile(sandboxRoot, "book.fb2"),
                WorkingDirectory = Path.Combine(sandboxRoot, "work")
            };

            var firstSubscriberEntered = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
            var releaseFirstSubscriber = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
            var callOrder = new List<string>();

            viewModel.ImportCompletedSuccessfully += async result =>
            {
                Assert.Equal(ImportJobStatusModel.Completed, result.Snapshot.Status);
                callOrder.Add("first-start");
                firstSubscriberEntered.TrySetResult();
                await releaseFirstSubscriber.Task;
                callOrder.Add("first-end");
            };
            viewModel.ImportCompletedSuccessfully += result =>
            {
                Assert.Equal(ImportJobStatusModel.Completed, result.Snapshot.Status);
                callOrder.Add("second");
                return Task.CompletedTask;
            };

            var importTask = viewModel.StartImportAsync();
            await firstSubscriberEntered.Task;

            Assert.Equal(["first-start"], callOrder);

            releaseFirstSubscriber.SetResult();
            await importTask;

            Assert.Equal(["first-start", "first-end", "second"], callOrder);
        }
        finally
        {
            try
            {
                Directory.Delete(sandboxRoot, true);
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
    public void LibraryBrowserViewModel_DefaultSortKeyIsTitle()
    {
        var viewModel = new LibraryBrowserViewModel();

        Assert.NotNull(viewModel.SelectedSortKey);
        Assert.Equal(BookSortModel.Title, viewModel.SelectedSortKey!.Key);
    }

    [Fact]
    public void LibraryBrowserViewModel_ExposesThreeSortKeys()
    {
        var viewModel = new LibraryBrowserViewModel();

        Assert.Equal(3, viewModel.AvailableSortKeys.Count);
        Assert.Contains(viewModel.AvailableSortKeys, o => o.Key == BookSortModel.Title);
        Assert.Contains(viewModel.AvailableSortKeys, o => o.Key == BookSortModel.Author);
        Assert.Contains(viewModel.AvailableSortKeys, o => o.Key == BookSortModel.DateAdded);
    }

    [Fact]
    public void LibraryBrowserViewModel_AppliesInitialSortKeyFromConstructorParameter()
    {
        var viewModel = new LibraryBrowserViewModel(initialSortKey: BookSortModel.DateAdded);

        Assert.NotNull(viewModel.SelectedSortKey);
        Assert.Equal(BookSortModel.DateAdded, viewModel.SelectedSortKey!.Key);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_SortKeyChangeIncludesSortByInCatalogRequest()
    {
        var service = new RecordingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        Assert.Equal(BookSortModel.Title, service.LastRequest!.SortBy);

        var authorOption = viewModel.AvailableSortKeys.First(o => o.Key == BookSortModel.Author);
        viewModel.SelectedSortKey = authorOption;
        await viewModel.RefreshAsync();

        Assert.Equal(BookSortModel.Author, service.LastRequest!.SortBy);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_SortDescendingToggleChangesSortDirectionInRequest()
    {
        var service = new RecordingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service);

        await viewModel.RefreshAsync();
        Assert.Equal(BookSortDirectionModel.Ascending, service.LastRequest!.SortDirection);

        await viewModel.ToggleSortDirectionCommand.ExecuteAsyncForTests();
        await viewModel.RefreshAsync();
        Assert.Equal(BookSortDirectionModel.Descending, service.LastRequest!.SortDirection);
    }

    [Fact]
    public async Task LibraryBrowserViewModel_AppliesInitialSortDescendingFromConstructorParameter()
    {
        var service = new RecordingLibraryCatalogService();
        var viewModel = new LibraryBrowserViewModel(service, initialSortDescending: true);

        await viewModel.RefreshAsync();
        Assert.Equal(BookSortDirectionModel.Descending, service.LastRequest!.SortDirection);
    }

    private sealed class FakeImportJobsService : IImportJobsService
    {
        public int TryGetSnapshotCalls { get; private set; }
        public int WaitCalls { get; private set; }
        public ulong? LastRemovedJobId { get; private set; }
        public StartImportRequestModel? LastStartRequest { get; private set; }
        private int _waitCalls;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(_waitCalls > 1
                ? new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Completed,
                        Message = "Completed",
                        Percent = 100,
                        TotalEntries = 1,
                        ProcessedEntries = 1,
                        ImportedEntries = 1,
                        FailedEntries = 0,
                        SkippedEntries = 0
                    },
                    Summary = new ImportSummaryModel
                    {
                        Mode = ImportModeModel.SingleFile,
                        TotalEntries = 1,
                        ImportedEntries = 1,
                        FailedEntries = 0,
                        SkippedEntries = 0,
                        Warnings = ["Watch for duplicates"]
                    }
                }
                : null);

        public Task<string> ValidateSourcesAsync(IReadOnlyList<string> sourcePaths, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(BuildSourceValidationMessage(sourcePaths));

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            TryGetSnapshotCalls++;
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Running",
                Percent = 0,
                TotalEntries = 1,
                ProcessedEntries = 0,
                ImportedEntries = 0,
                FailedEntries = 0,
                SkippedEntries = 0
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRemovedJobId = jobId;
            return Task.FromResult(true);
        }

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastStartRequest = request;
            return Task.FromResult(42UL);
        }

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
        {
            WaitCalls++;
            _waitCalls++;
            return Task.FromResult(_waitCalls > 1);
        }

        public async Task<ImportJobResultModel> WaitForCompletionAsync(
            ulong jobId,
            TimeSpan timeout,
            TimeSpan waitTimeout,
            Action<ImportJobSnapshotModel>? onProgress,
            CancellationToken cancellationToken)
        {
            while (!await WaitAsync(jobId, timeout, waitTimeout, cancellationToken))
            {
                var snapshot = await TryGetSnapshotAsync(jobId, timeout, cancellationToken);
                if (snapshot is not null)
                {
                    onProgress?.Invoke(snapshot);
                }
            }

            return (await TryGetResultAsync(jobId, timeout, cancellationToken))!;
        }
    }

    private sealed class FailingImportJobsService : IImportJobsService
    {
        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(null);

        public Task<string> ValidateSourcesAsync(IReadOnlyList<string> sourcePaths, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(string.Empty);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobSnapshotModel?>(null);

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => throw new InvalidOperationException("transport failed");

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public async Task<ImportJobResultModel> WaitForCompletionAsync(
            ulong jobId,
            TimeSpan timeout,
            TimeSpan waitTimeout,
            Action<ImportJobSnapshotModel>? onProgress,
            CancellationToken cancellationToken)
        {
            await WaitAsync(jobId, timeout, waitTimeout, cancellationToken);
            var result = await TryGetResultAsync(jobId, timeout, cancellationToken);
            throw new InvalidOperationException(result is null ? "No terminal result." : "WaitForCompletionAsync should not be called.");
        }
    }

    private sealed class SnapshotOnlyImportJobsService : IImportJobsService
    {
        public int RefreshSnapshotCalls { get; private set; }
        public bool ReturnTerminalResult { get; set; } = true;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(ReturnTerminalResult
                ? new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Completed,
                        Message = "Completed",
                        Percent = 100,
                        TotalEntries = 1,
                        ProcessedEntries = 1,
                        ImportedEntries = 1,
                        FailedEntries = 0,
                        SkippedEntries = 0
                    }
                }
                : null);

        public Task<string> ValidateSourcesAsync(IReadOnlyList<string> sourcePaths, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(string.Empty);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            RefreshSnapshotCalls++;
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Still running",
                Percent = 25,
                TotalEntries = 4,
                ProcessedEntries = 1,
                ImportedEntries = 1,
                FailedEntries = 0,
                SkippedEntries = 0
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(64UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(RefreshSnapshotCalls > 0);

        public async Task<ImportJobResultModel> WaitForCompletionAsync(
            ulong jobId,
            TimeSpan timeout,
            TimeSpan waitTimeout,
            Action<ImportJobSnapshotModel>? onProgress,
            CancellationToken cancellationToken)
        {
            while (!await WaitAsync(jobId, timeout, waitTimeout, cancellationToken))
            {
                var snapshot = await TryGetSnapshotAsync(jobId, timeout, cancellationToken);
                if (snapshot is not null)
                {
                    onProgress?.Invoke(snapshot);
                }
            }

            return (await TryGetResultAsync(jobId, timeout, cancellationToken))!;
        }
    }

    private sealed class CancelAwareImportJobsService : IImportJobsService
    {
        private readonly TaskCompletionSource _started = new();
        private readonly TaskCompletionSource _polling = new();
        private volatile bool _cancelled;

        public int CancelCalls { get; private set; }
        public int StartCalls { get; private set; }
        public Task WaitForStartAsync() => _started.Task;
        public Task WaitForPollingAsync() => _polling.Task;

        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            CancelCalls++;
            _cancelled = true;
            return Task.FromResult(true);
        }

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _started.TrySetResult();

            if (_cancelled)
            {
                return Task.FromResult<ImportJobResultModel?>(new ImportJobResultModel
                {
                    Snapshot = new ImportJobSnapshotModel
                    {
                        JobId = jobId,
                        Status = ImportJobStatusModel.Cancelled,
                        Message = "Cancelled",
                        Percent = 33,
                        TotalEntries = 3,
                        ProcessedEntries = 1,
                        ImportedEntries = 0,
                        FailedEntries = 1,
                        SkippedEntries = 0
                    }
                });
            }

            return Task.FromResult<ImportJobResultModel?>(null);
        }

        public Task<string> ValidateSourcesAsync(IReadOnlyList<string> sourcePaths, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(string.Empty);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _polling.TrySetResult();
            return Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Running",
                Percent = 0,
                TotalEntries = 3,
                ProcessedEntries = 0,
                ImportedEntries = 0,
                FailedEntries = 0,
                SkippedEntries = 0
            });
        }

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            StartCalls++;
            return Task.FromResult(77UL);
        }

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
        {
            _started.TrySetResult();
            return Task.FromResult(_cancelled);
        }

        public async Task<ImportJobResultModel> WaitForCompletionAsync(
            ulong jobId,
            TimeSpan timeout,
            TimeSpan waitTimeout,
            Action<ImportJobSnapshotModel>? onProgress,
            CancellationToken cancellationToken)
        {
            while (!await WaitAsync(jobId, timeout, waitTimeout, cancellationToken))
            {
                var snapshot = await TryGetSnapshotAsync(jobId, timeout, cancellationToken);
                if (snapshot is not null)
                {
                    onProgress?.Invoke(snapshot);
                }

                await Task.Delay(20, cancellationToken);
            }

            return (await TryGetResultAsync(jobId, timeout, cancellationToken))!;
        }
    }

    private sealed class FakePathSelectionService : IPathSelectionService
    {
        public IReadOnlyList<string> SelectedSourcePaths { get; init; } = [];
        public string? SelectedSourceDirectory { get; init; }
        public string? SelectedWorkingDirectory { get; init; }
        public string? SelectedExportPath { get; init; }
        public string? LastSuggestedExportFileName { get; private set; }

        public Task<IReadOnlyList<string>> PickSourceFilesAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedSourcePaths);

        public Task<string?> PickSourceDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedSourceDirectory);

        public Task<string?> PickWorkingDirectoryAsync(CancellationToken cancellationToken)
            => Task.FromResult(SelectedWorkingDirectory);

        public Task<string?> PickExecutableFileAsync(string title, CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);

        public Task<string?> PickExportDestinationAsync(string suggestedFileName, CancellationToken cancellationToken)
        {
            LastSuggestedExportFileName = suggestedFileName;
            return Task.FromResult(SelectedExportPath);
        }
    }

    private sealed class EmptyImportJobsService : IImportJobsService
    {
        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(new ImportJobResultModel
            {
                Snapshot = new ImportJobSnapshotModel
                {
                    JobId = jobId,
                    Status = ImportJobStatusModel.Failed,
                    Message = "Import completed without importing any supported books.",
                    Percent = 0,
                    TotalEntries = 0,
                    ProcessedEntries = 0,
                    ImportedEntries = 0,
                    FailedEntries = 0,
                    SkippedEntries = 0
                },
                Summary = new ImportSummaryModel
                {
                    Mode = ImportModeModel.Batch,
                    TotalEntries = 0,
                    ImportedEntries = 0,
                    FailedEntries = 0,
                    SkippedEntries = 0
                }
            });

        public Task<string> ValidateSourcesAsync(IReadOnlyList<string> sourcePaths, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(string.Empty);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Prepared import workload",
                Percent = 0,
                TotalEntries = 0,
                ProcessedEntries = 0,
                ImportedEntries = 0,
                FailedEntries = 0,
                SkippedEntries = 0
            });

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(91UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public async Task<ImportJobResultModel> WaitForCompletionAsync(
            ulong jobId,
            TimeSpan timeout,
            TimeSpan waitTimeout,
            Action<ImportJobSnapshotModel>? onProgress,
            CancellationToken cancellationToken)
        {
            await WaitAsync(jobId, timeout, waitTimeout, cancellationToken);
            var result = await TryGetResultAsync(jobId, timeout, cancellationToken);
            return result!;
        }
    }

    private sealed class CancellationResidueImportJobsService : IImportJobsService
    {
        public Task<bool> CancelAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ImportJobResultModel?> TryGetResultAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobResultModel?>(new ImportJobResultModel
            {
                Snapshot = new ImportJobSnapshotModel
                {
                    JobId = jobId,
                    Status = ImportJobStatusModel.Cancelled,
                    Message = "Import was cancelled. Some managed files could not be removed during rollback.",
                    Percent = 50,
                    TotalEntries = 2,
                    ProcessedEntries = 2,
                    ImportedEntries = 0,
                    FailedEntries = 1,
                    SkippedEntries = 0
                },
                Summary = new ImportSummaryModel
                {
                    Mode = ImportModeModel.Batch,
                    TotalEntries = 2,
                    ImportedEntries = 0,
                    FailedEntries = 1,
                    SkippedEntries = 0,
                    Warnings =
                    [
                        "Cancellation rollback left managed artifact 'C:/Library/Books/0000000011/book.epub' on disk because Path could not be resolved safely within the library root."
                    ]
                },
                Error = new DomainErrorModel
                {
                    Code = ImportErrorCodeModel.Cancellation,
                    Message = "Import was cancelled. Some managed files could not be removed during rollback."
                }
            });

        public Task<string> ValidateSourcesAsync(IReadOnlyList<string> sourcePaths, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(string.Empty);

        public Task<ImportJobSnapshotModel?> TryGetSnapshotAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<ImportJobSnapshotModel?>(new ImportJobSnapshotModel
            {
                JobId = jobId,
                Status = ImportJobStatusModel.Running,
                Message = "Running",
                Percent = 0,
                TotalEntries = 2,
                ProcessedEntries = 0,
                ImportedEntries = 0,
                FailedEntries = 0,
                SkippedEntries = 0
            });

        public Task<bool> RemoveAsync(ulong jobId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public Task<ulong> StartAsync(StartImportRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(81UL);

        public Task<bool> WaitAsync(ulong jobId, TimeSpan timeout, TimeSpan waitTimeout, CancellationToken cancellationToken)
            => Task.FromResult(true);

        public async Task<ImportJobResultModel> WaitForCompletionAsync(
            ulong jobId,
            TimeSpan timeout,
            TimeSpan waitTimeout,
            Action<ImportJobSnapshotModel>? onProgress,
            CancellationToken cancellationToken)
        {
            await WaitAsync(jobId, timeout, waitTimeout, cancellationToken);
            var result = await TryGetResultAsync(jobId, timeout, cancellationToken);
            return result!;
        }
    }

    private static string CreateSupportedSourceFile(string sandboxRoot, string fileName)
    {
        var sourcePath = Path.Combine(sandboxRoot, fileName);
        File.WriteAllText(sourcePath, "content");
        return sourcePath;
    }

    private static string BuildSourceValidationMessage(IReadOnlyList<string> sourcePaths)
    {
        var hasImportableSource = false;

        foreach (var sourcePath in sourcePaths)
        {
            if (Directory.Exists(sourcePath))
            {
                if (Directory.EnumerateFiles(sourcePath, "*", SearchOption.AllDirectories).Any(IsSupportedImportFile))
                {
                    hasImportableSource = true;
                    continue;
                }

                return "Supported source types are .fb2, .epub, and .zip, or a directory containing them.";
            }

            if (!File.Exists(sourcePath))
            {
                return "A selected source does not exist.";
            }

            if (IsSupportedImportFile(sourcePath))
            {
                hasImportableSource = true;
            }
        }

        return hasImportableSource
            ? string.Empty
            : "Supported source types are .fb2, .epub, and .zip, or a directory containing them.";
    }

    private static bool IsSupportedImportFile(string path)
    {
        var extension = Path.GetExtension(path);
        return extension.Equals(".fb2", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".epub", StringComparison.OrdinalIgnoreCase)
            || extension.Equals(".zip", StringComparison.OrdinalIgnoreCase);
    }

    private static async Task WaitForConditionAsync(Func<bool> condition, int timeoutMilliseconds = 3000)
    {
        var deadline = DateTime.UtcNow.AddMilliseconds(timeoutMilliseconds);
        while (DateTime.UtcNow < deadline)
        {
            if (condition())
            {
                return;
            }

            await Task.Delay(25);
        }

        Assert.True(condition(), "Timed out waiting for the expected state.");
    }

    private sealed class FakeLibraryCatalogService : ILibraryCatalogService
    {
        public long? LastExportBookId { get; private set; }
        public string? LastExportPath { get; private set; }
        public BookFormatModel? LastExportFormat { get; private set; }
        public TimeSpan? LastExportTimeout { get; private set; }
        public long? LastTrashedBookId { get; private set; }
        public LibraryStatisticsModel Statistics { get; init; } = new()
        {
            BookCount = 1
        };

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = 1,
                AvailableLanguages = ["en"],
                Statistics = Statistics,
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Roadside Picnic",
                        Authors = ["Arkady Strugatsky"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000001/book.epub",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(new BookDetailsModel
            {
                BookId = bookId,
                Title = "Roadside Picnic",
                Authors = ["Arkady Strugatsky"],
                Language = "en",
                Series = "Noon Universe",
                SeriesIndex = 1.5,
                Publisher = "Macmillan",
                Year = 1972,
                Isbn = "978-5-17-000000-1",
                Tags = [],
                Genres = ["sci-fi", "adventure"],
                Description = "Aliens land only in one city.",
                Identifier = "details-id",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/book.epub",
                SizeBytes = 4096,
                Storage = new BookStorageInfoModel
                {
                    ManagedRelativePath = "Books/0000000001/book.epub",
                    HasContentHash = true
                },
                AddedAtUtc = DateTimeOffset.UtcNow
            });

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
        {
            LastExportBookId = bookId;
            LastExportPath = destinationPath;
            LastExportFormat = exportFormat;
            LastExportTimeout = timeout;
            return Task.FromResult<string?>(destinationPath);
        }

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastTrashedBookId = bookId;
            return Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
        }
    }

    private sealed class EmptyLibraryCatalogService : ILibraryCatalogService
    {
        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel());

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(null);
    }

    private sealed class RefreshPreservingLibraryCatalogService : ILibraryCatalogService
    {
        public int GetBookDetailsCalls { get; private set; }

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = 1,
                AvailableLanguages = ["en"],
                Statistics = new LibraryStatisticsModel
                {
                    BookCount = 1
                },
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Roadside Picnic",
                        Authors = ["Arkady Strugatsky"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000001/book.epub",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            GetBookDetailsCalls++;
            return Task.FromResult<BookDetailsModel?>(new BookDetailsModel
            {
                BookId = bookId,
                Title = "Roadside Picnic",
                Authors = ["Arkady Strugatsky"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/book.epub",
                SizeBytes = 4096,
                Storage = new BookStorageInfoModel
                {
                    ManagedRelativePath = "Books/0000000001/book.epub",
                    HasContentHash = true
                },
                AddedAtUtc = DateTimeOffset.UtcNow
            });
        }

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class NotFoundDetailsLibraryCatalogService : ILibraryCatalogService
    {
        private readonly FakeLibraryCatalogService _inner = new();

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken) =>
            _inner.ListBooksAsync(request, timeout, cancellationToken);

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken) =>
            Task.FromException<BookDetailsModel?>(new LibraryCatalogDomainException(new LibraryCatalogDomainErrorModel
            {
                Code = LibraryCatalogErrorCodeModel.NotFound,
                Message = $"Book {bookId} was not found."
            }));

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken) =>
            _inner.ExportBookAsync(bookId, destinationPath, exportFormat, timeout, cancellationToken);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken) =>
            _inner.MoveBookToTrashAsync(bookId, timeout, cancellationToken);
    }

    private sealed class NotFoundExportLibraryCatalogService : ILibraryCatalogService
    {
        private readonly FakeLibraryCatalogService _inner = new();

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken) =>
            _inner.ListBooksAsync(request, timeout, cancellationToken);

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken) =>
            _inner.GetBookDetailsAsync(bookId, timeout, cancellationToken);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken) =>
            Task.FromException<string?>(new LibraryCatalogDomainException(new LibraryCatalogDomainErrorModel
            {
                Code = LibraryCatalogErrorCodeModel.NotFound,
                Message = $"Book {bookId} was not found for export."
            }));

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken) =>
            _inner.MoveBookToTrashAsync(bookId, timeout, cancellationToken);
    }

    private sealed class NotFoundDeleteLibraryCatalogService : ILibraryCatalogService
    {
        private readonly FakeLibraryCatalogService _inner = new();

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken) =>
            _inner.ListBooksAsync(request, timeout, cancellationToken);

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken) =>
            _inner.GetBookDetailsAsync(bookId, timeout, cancellationToken);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken) =>
            _inner.ExportBookAsync(bookId, destinationPath, exportFormat, timeout, cancellationToken);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken) =>
            Task.FromException<DeleteBookResultModel?>(new LibraryCatalogDomainException(new LibraryCatalogDomainErrorModel
            {
                Code = LibraryCatalogErrorCodeModel.NotFound,
                Message = $"Book {bookId} was not found for deletion."
            }));
    }

    private sealed class RecordingLibraryCatalogService : ILibraryCatalogService
    {
        public BookListRequestModel? LastRequest { get; private set; }
        public (long BookId, string DestinationPath)? LastExportRequest { get; private set; }
        public IReadOnlyList<string> AvailableLanguages { get; init; } = [];
        public IReadOnlyList<string> AvailableGenres { get; init; } = [];

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRequest = request;
            return Task.FromResult(new BookListPageModel
            {
                AvailableLanguages = AvailableLanguages,
                AvailableGenres = AvailableGenres
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
        {
            LastExportRequest = (bookId, destinationPath);
            return Task.FromResult<string?>(destinationPath);
        }

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class QueryFilteringLibraryCatalogService : ILibraryCatalogService
    {
        private static readonly IReadOnlyList<BookListItemModel> AllBooks =
        [
            new BookListItemModel
            {
                BookId = 1,
                Title = "Roadside Picnic",
                Authors = ["Arkady Strugatsky"],
                Language = "en",
                Tags = ["sci-fi"],
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/roadside.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new BookListItemModel
            {
                BookId = 2,
                Title = "Monday Begins on Saturday",
                Authors = ["Arkady Strugatsky", "Boris Strugatsky"],
                Language = "ru",
                Tags = ["adventure"],
                Format = BookFormatModel.Fb2,
                ManagedPath = "Books/0000000002/monday.fb2",
                AddedAtUtc = DateTimeOffset.UtcNow
            }
        ];

        public int ListCalls { get; private set; }
        public LibraryStatisticsModel Statistics { get; init; } = new()
        {
            BookCount = (ulong)AllBooks.Count
        };

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            ListCalls++;

            IEnumerable<BookListItemModel> filtered = AllBooks;
            if (!string.IsNullOrWhiteSpace(request.Text))
            {
                filtered = filtered.Where(book => book.Title.Contains(request.Text, StringComparison.OrdinalIgnoreCase));
            }

            if (request.Languages.Count > 0)
            {
                filtered = filtered.Where(book => request.Languages.Contains(book.Language, StringComparer.OrdinalIgnoreCase));
            }

            if (request.Genres.Count > 0)
            {
                filtered = filtered.Where(book => book.Tags.Any(tag => request.Genres.Contains(tag, StringComparer.OrdinalIgnoreCase)));
            }

            var filteredItems = filtered.ToArray();
            return Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)filteredItems.Length,
                AvailableLanguages = AllBooks
                    .Select(book => book.Language)
                    .Distinct(StringComparer.OrdinalIgnoreCase)
                    .OrderBy(language => language, StringComparer.OrdinalIgnoreCase)
                    .ToArray(),
                AvailableGenres = AllBooks
                    .SelectMany(book => book.Tags)
                    .Distinct(StringComparer.OrdinalIgnoreCase)
                    .OrderBy(tag => tag, StringComparer.OrdinalIgnoreCase)
                    .ToArray(),
                Items = filteredItems,
                Statistics = Statistics
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class OmittingSelectedFacetLibraryCatalogService : ILibraryCatalogService
    {
        private static readonly BookListItemModel[] AllBooks =
        [
            new BookListItemModel
            {
                BookId = 1,
                Title = "English Book",
                Authors = ["Author One"],
                Language = "en",
                Tags = ["adventure"],
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000001001/en.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new BookListItemModel
            {
                BookId = 2,
                Title = "Russian Book",
                Authors = ["Author Two"],
                Language = "ru",
                Tags = ["classic"],
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000001002/ru.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            }
        ];

        public int ListCalls { get; private set; }
        public BookListRequestModel? LastRequest { get; private set; }

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRequest = request;
            ListCalls++;

            var filteredItems = AllBooks
                .Where(book => request.Languages.Count == 0
                    || request.Languages.Contains(book.Language, StringComparer.OrdinalIgnoreCase))
                .ToArray();

            IReadOnlyList<string> availableLanguages = request.Languages.Count == 0
                ? ["en", "ru"]
                : ["en"];

            return Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)filteredItems.Length,
                AvailableLanguages = availableLanguages,
                AvailableGenres = ["adventure", "classic"],
                Items = filteredItems
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class CoverAwareLibraryCatalogService : ILibraryCatalogService
    {
        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = 1,
                AvailableLanguages = ["en"],
                Statistics = null,
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Covered Book",
                        Authors = ["Author"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000001/book.epub",
                        CoverPath = "Covers/0000000001.png",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(new BookDetailsModel
            {
                BookId = bookId,
                Title = "Covered Book",
                Authors = ["Author"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/book.epub",
                CoverPath = "Covers/0000000001.png",
                SizeBytes = 1024,
                Storage = new BookStorageInfoModel
                {
                    ManagedRelativePath = "Books/0000000001/book.epub",
                    CoverRelativePath = "Covers/0000000001.png",
                    HasContentHash = true
                },
                AddedAtUtc = DateTimeOffset.UtcNow
            });

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class CompressedFb2LibraryCatalogService : ILibraryCatalogService
    {
        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = 1,
                AvailableLanguages = ["en"],
                Statistics = null,
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Roadside Picnic",
                        Authors = ["Arkady Strugatsky"],
                        Language = "en",
                        Format = BookFormatModel.Fb2,
                        ManagedPath = "Books/0000000001/book.fb2.gz",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class InvalidMetadataLibraryCatalogService : ILibraryCatalogService
    {
        public string? LastExportPath { get; private set; }
        public BookFormatModel? LastExportFormat { get; private set; }
        public TimeSpan? LastExportTimeout { get; private set; }

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = 1,
                AvailableLanguages = ["en"],
                Statistics = null,
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 41,
                        Title = "Roadside: Picnic / Director's Cut?",
                        Authors = ["Arkady*Boris|Strugatsky"],
                        Language = "en",
                        Format = BookFormatModel.Fb2,
                        ManagedPath = "Books/0000000041/book.fb2",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(new BookDetailsModel
            {
                BookId = bookId,
                Title = "Roadside: Picnic / Director's Cut?",
                Authors = ["Arkady*Boris|Strugatsky"],
                Language = "en",
                Format = BookFormatModel.Fb2,
                ManagedPath = "Books/0000000041/book.fb2",
                SizeBytes = 512,
                Storage = new BookStorageInfoModel
                {
                    ManagedRelativePath = "Books/0000000041/book.fb2",
                    HasContentHash = true
                },
                AddedAtUtc = DateTimeOffset.UtcNow
            });

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
        {
            LastExportPath = destinationPath;
            LastExportFormat = exportFormat;
            LastExportTimeout = timeout;
            return Task.FromResult<string?>(destinationPath);
        }

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class FixedCoverLibraryCatalogService(string coverPath) : ILibraryCatalogService
    {
        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = 1,
                AvailableLanguages = ["en"],
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Test Book",
                        Authors = ["Author"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000001/book.epub",
                        CoverPath = coverPath,
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(null);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(null);
    }

    private sealed class RecordingCoverImageLoader : ICoverImageLoader
    {
        public List<string> LoadedPaths { get; } = [];

        public Avalonia.Media.IImage? Load(string absolutePath)
        {
            LoadedPaths.Add(absolutePath);
            return null;
        }
    }

    private sealed class StubCoverImageLoader : ICoverImageLoader
    {
        public Avalonia.Media.IImage? Load(string absolutePath) => new StubCoverImage();
    }

    private sealed class ThrowingCoverImageLoader(Exception error) : ICoverImageLoader
    {
        public Avalonia.Media.IImage? Load(string absolutePath) => throw error;
    }

    private sealed class StubCoverImage : IImage
    {
        public Size Size => new(1, 1);

        public void Draw(DrawingContext context, Rect sourceRect, Rect destRect)
        {
        }
    }

    private sealed class StatisticsFailureLibraryCatalogService : ILibraryCatalogService
    {
        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = 1,
                AvailableLanguages = ["en"],
                Statistics = null,
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Roadside Picnic",
                        Authors = ["Arkady Strugatsky"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000001/book.epub",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class PagingLibraryCatalogService : ILibraryCatalogService
    {
        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            if (request.Offset == 0)
            {
                return Task.FromResult(new BookListPageModel
                {
                    TotalCount = 3,
                    AvailableLanguages = ["en"],
                    Items =
                    [
                        new BookListItemModel
                        {
                            BookId = 1,
                            Title = "Alpha",
                            Authors = ["Author One"],
                            Language = "en",
                            Format = BookFormatModel.Epub,
                            ManagedPath = "Books/0000000001/alpha.epub",
                            AddedAtUtc = DateTimeOffset.UtcNow
                        },
                        new BookListItemModel
                        {
                            BookId = 2,
                            Title = "Beta",
                            Authors = ["Author Two"],
                            Language = "en",
                            Format = BookFormatModel.Epub,
                            ManagedPath = "Books/0000000002/beta.epub",
                            AddedAtUtc = DateTimeOffset.UtcNow
                        }
                    ]
                });
            }

            return Task.FromResult(new BookListPageModel
            {
                TotalCount = 3,
                AvailableLanguages = ["en"],
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 3,
                        Title = "Gamma",
                        Authors = ["Author Three"],
                        Language = "en",
                        Format = BookFormatModel.Fb2,
                        ManagedPath = "Books/0000000003/gamma.fb2",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class ExactPageLibraryCatalogService : ILibraryCatalogService
    {
        public BookListRequestModel? LastRequest { get; private set; }

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            LastRequest = request;
            return Task.FromResult(new BookListPageModel
            {
                TotalCount = 2,
                AvailableLanguages = ["en"],
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Alpha",
                        Authors = ["Author One"],
                        Language = "en",
                        Format = BookFormatModel.Epub,
                        ManagedPath = "Books/0000000001/alpha.epub",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    },
                    new BookListItemModel
                    {
                        BookId = 2,
                        Title = "Beta",
                        Authors = ["Author Two"],
                        Language = "en",
                        Format = BookFormatModel.Fb2,
                        ManagedPath = "Books/0000000002/beta.fb2",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(
            long bookId,
            string destinationPath,
            BookFormatModel? exportFormat,
            TimeSpan timeout,
            CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class FailingLoadMoreLibraryCatalogService : ILibraryCatalogService
    {
        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            if (request.Offset == 0)
            {
                return Task.FromResult(new BookListPageModel
                {
                    TotalCount = 3,
                    AvailableLanguages = ["en"],
                    Items =
                    [
                        new BookListItemModel
                        {
                            BookId = 1,
                            Title = "Alpha",
                            Authors = ["Author One"],
                            Language = "en",
                            Format = BookFormatModel.Epub,
                            ManagedPath = "Books/0000000001/alpha.epub",
                            AddedAtUtc = DateTimeOffset.UtcNow
                        },
                        new BookListItemModel
                        {
                            BookId = 2,
                            Title = "Beta",
                            Authors = ["Author Two"],
                            Language = "en",
                            Format = BookFormatModel.Epub,
                            ManagedPath = "Books/0000000002/beta.epub",
                            AddedAtUtc = DateTimeOffset.UtcNow
                        }
                    ]
                });
            }

            throw new InvalidOperationException("load more failed");
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class PagedLanguageLibraryCatalogService : ILibraryCatalogService
    {
        private readonly IReadOnlyList<BookListItemModel> _books =
        [
            new()
            {
                BookId = 1,
                Title = "Alpha",
                Authors = ["Author One"],
                Language = "en",
                Tags = ["adventure"],
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/alpha.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new()
            {
                BookId = 2,
                Title = "Beta",
                Authors = ["Author Two"],
                Language = "en",
                Tags = ["sci-fi"],
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000002/beta.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new()
            {
                BookId = 3,
                Title = "Gamma",
                Authors = ["Author Three"],
                Language = "ru",
                Tags = ["sci-fi"],
                Format = BookFormatModel.Fb2,
                ManagedPath = "Books/0000000003/gamma.fb2",
                AddedAtUtc = DateTimeOffset.UtcNow
            }
        ];

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            var items = _books
                .Skip((int)request.Offset)
                .Take((int)request.Limit)
                .ToArray();

            return Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)_books.Count,
                AvailableLanguages = ["en", "ru"],
                AvailableGenres = ["adventure", "sci-fi"],
                Items = items
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
    }

    private sealed class LanguageDeletionLibraryCatalogService : ILibraryCatalogService
    {
        private readonly List<BookListItemModel> _books =
        [
            new()
            {
                BookId = 1,
                Title = "Alpha",
                Authors = ["Author One"],
                Language = "en",
                Tags = ["adventure"],
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/alpha.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new()
            {
                BookId = 2,
                Title = "Gamma",
                Authors = ["Author Three"],
                Language = "ru",
                Tags = ["sci-fi"],
                Format = BookFormatModel.Fb2,
                ManagedPath = "Books/0000000002/gamma.fb2",
                AddedAtUtc = DateTimeOffset.UtcNow
            }
        ];

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            var items = _books
                .Skip((int)request.Offset)
                .Take((int)request.Limit)
                .ToArray();
            return Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)_books.Count,
                AvailableLanguages = _books
                    .Select(book => book.Language)
                    .Distinct(StringComparer.OrdinalIgnoreCase)
                    .OrderBy(language => language, StringComparer.OrdinalIgnoreCase)
                    .ToArray(),
                AvailableGenres = _books
                    .SelectMany(book => book.Tags)
                    .Distinct(StringComparer.OrdinalIgnoreCase)
                    .OrderBy(tag => tag, StringComparer.OrdinalIgnoreCase)
                    .ToArray(),
                Items = items
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _books.RemoveAll(book => book.BookId == bookId);
            return Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
        }
    }

    private sealed class DeleteAfterDeepScrollLibraryCatalogService : ILibraryCatalogService
    {
        private readonly List<BookListItemModel> _books =
        [
            new()
            {
                BookId = 1,
                Title = "Alpha",
                Authors = ["Author One"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/alpha.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new()
            {
                BookId = 2,
                Title = "Beta",
                Authors = ["Author Two"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000002/beta.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new()
            {
                BookId = 3,
                Title = "Gamma",
                Authors = ["Author Three"],
                Language = "en",
                Format = BookFormatModel.Fb2,
                ManagedPath = "Books/0000000003/gamma.fb2",
                AddedAtUtc = DateTimeOffset.UtcNow
            },
            new()
            {
                BookId = 4,
                Title = "Delta",
                Authors = ["Author Four"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000004/delta.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            }
        ];

        public int ListCalls { get; private set; }
        public List<BookListRequestModel> Requests { get; } = [];

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
        {
            ListCalls++;
            Requests.Add(request);

            var items = _books
                .Skip((int)request.Offset)
                .Take((int)request.Limit)
                .ToArray();
            return Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)_books.Count,
                AvailableLanguages = ["en"],
                Items = items,
                Statistics = new LibraryStatisticsModel
                {
                    BookCount = (ulong)_books.Count
                }
            });
        }

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _books.RemoveAll(book => book.BookId == bookId);
            return Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin
            });
        }
    }

    private sealed class OrphanedDeleteLibraryCatalogService : ILibraryCatalogService
    {
        private readonly List<BookListItemModel> _books =
        [
            new()
            {
                BookId = 1,
                Title = "Orphan",
                Authors = ["Author"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/orphan.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            }
        ];

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)_books.Count,
                AvailableLanguages = ["en"],
                Items = _books.ToArray()
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _books.RemoveAll(book => book.BookId == bookId);
            return Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.RecycleBin,
                HasOrphanedFiles = true
            });
        }
    }

    private sealed class ManagedTrashOrphanedDeleteLibraryCatalogService : ILibraryCatalogService
    {
        private readonly List<BookListItemModel> _books =
        [
            new BookListItemModel
            {
                BookId = 1,
                Title = "Orphaned Trash Book",
                Authors = ["Author"],
                Language = "en",
                Format = BookFormatModel.Epub,
                ManagedPath = "Books/0000000001/orphan.epub",
                AddedAtUtc = DateTimeOffset.UtcNow
            }
        ];

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = (ulong)_books.Count,
                AvailableLanguages = ["en"],
                Items = _books.ToArray()
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<string?>(destinationPath);

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
        {
            _books.RemoveAll(book => book.BookId == bookId);
            return Task.FromResult<DeleteBookResultModel?>(new DeleteBookResultModel
            {
                BookId = bookId,
                Destination = DeleteDestinationModel.ManagedTrash,
                HasOrphanedFiles = true
            });
        }
    }

    private sealed class GatedExportLibraryCatalogService : ILibraryCatalogService
    {
        private readonly Task _gate;

        public GatedExportLibraryCatalogService(Task gate)
        {
            _gate = gate;
        }

        public Task<BookListPageModel> ListBooksAsync(BookListRequestModel request, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult(new BookListPageModel
            {
                TotalCount = 1,
                AvailableLanguages = ["en"],
                Items =
                [
                    new BookListItemModel
                    {
                        BookId = 1,
                        Title = "Slow Book",
                        Authors = ["Slow Author"],
                        Language = "en",
                        Format = BookFormatModel.Fb2,
                        ManagedPath = "Books/0000000001/book.fb2",
                        AddedAtUtc = DateTimeOffset.UtcNow
                    }
                ]
            });

        public Task<BookDetailsModel?> GetBookDetailsAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<BookDetailsModel?>(null);

        public async Task<string?> ExportBookAsync(long bookId, string destinationPath, BookFormatModel? exportFormat, TimeSpan timeout, CancellationToken cancellationToken)
        {
            await _gate;
            return destinationPath;
        }

        public Task<DeleteBookResultModel?> MoveBookToTrashAsync(long bookId, TimeSpan timeout, CancellationToken cancellationToken)
            => Task.FromResult<DeleteBookResultModel?>(null);
    }

    // ── ShellViewModel / converter probe tests ──────────────────────────────────────────────

    [Fact]
    public async Task ShellViewModel_ConverterProbeBlocksSaveWhileRunning()
    {
        var probeTcs = new TaskCompletionSource<Fb2ProbeResult>();
        var viewModel = MakeTestShellViewModel(probe: async (_, ct) => await probeTcs.Task);

        viewModel.Fb2CngExecutablePath = @"C:\fake\fb2cng.exe";

        Assert.True(viewModel.IsConverterProbeInProgress);
        Assert.False(viewModel.SavePreferencesCommand.CanExecute(null));

        probeTcs.SetResult(Fb2ProbeResult.Success);
        await viewModel.WaitForConverterProbeAsync();

        Assert.False(viewModel.IsConverterProbeInProgress);
        Assert.True(viewModel.SavePreferencesCommand.CanExecute(null));
    }

    [Fact]
    public async Task ShellViewModel_ConverterProbeSuccessEnablesSaveAndClearsMessage()
    {
        var viewModel = MakeTestShellViewModel(
            probe: (_, _) => Task.FromResult(Fb2ProbeResult.Success));

        viewModel.Fb2CngExecutablePath = @"C:\fake\fb2cng.exe";
        await viewModel.WaitForConverterProbeAsync();

        Assert.False(viewModel.IsConverterProbeInProgress);
        Assert.False(viewModel.HasConverterValidationError);
        Assert.True(viewModel.ShowConverterHelperText);
        Assert.True(viewModel.SavePreferencesCommand.CanExecute(null));
    }

    [Fact]
    public async Task ShellViewModel_ConverterProbeFailureShowsErrorAndBlocksSave()
    {
        var failResult = new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.ExecutionFailed, Detail = "1" };
        var viewModel = MakeTestShellViewModel(probe: (_, _) => Task.FromResult(failResult));

        viewModel.Fb2CngExecutablePath = @"C:\fake\fb2cng.exe";
        await viewModel.WaitForConverterProbeAsync();

        Assert.False(viewModel.IsConverterProbeInProgress);
        Assert.True(viewModel.HasConverterValidationError);
        Assert.False(string.IsNullOrWhiteSpace(viewModel.ConverterValidationMessage));
        Assert.False(viewModel.SavePreferencesCommand.CanExecute(null));
    }

    [Fact]
    public void ShellViewModel_DoesNotProbeSavedConverterPathOnConstruction()
    {
        var probeCallCount = 0;
        var session = new ShellSession(
            hostLifetime: NullLifetime.Instance,
            hostOptions: new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\fake\host.exe",
                PipePath = @"\\.\pipe\test",
                LibraryRoot = @"C:\fake\lib"
            },
            importJobs: new FakeImportJobsService());

        var viewModel = new ShellViewModel(
            session,
            savedPreferences: new UiPreferencesSnapshot
            {
                Fb2CngExecutablePath = @"C:\fake\fb2cng.exe"
            },
            converterProbe: (_, _) =>
            {
                probeCallCount++;
                return Task.FromResult(Fb2ProbeResult.Success);
            });

        Assert.Equal(0, probeCallCount);
        Assert.False(viewModel.IsConverterProbeInProgress);
        Assert.False(viewModel.HasConverterValidationError);
        Assert.True(viewModel.SavePreferencesCommand.CanExecute(null));
    }

    [Fact]
    public void ShellViewModel_EmptyPathSkipsProbeAndEnablesSave()
    {
        var probeCallCount = 0;
        var viewModel = MakeTestShellViewModel(probe: (_, _) => { probeCallCount++; return Task.FromResult(Fb2ProbeResult.Success); });

        viewModel.Fb2CngExecutablePath = string.Empty;

        Assert.Equal(0, probeCallCount);
        Assert.False(viewModel.IsConverterProbeInProgress);
        Assert.False(viewModel.HasConverterValidationError);
        Assert.True(viewModel.SavePreferencesCommand.CanExecute(null));
    }

    [Fact]
    public void ShellViewModel_RelativePathFailsSyncCheckWithoutProbe()
    {
        var probeCallCount = 0;
        var viewModel = MakeTestShellViewModel(probe: (_, _) => { probeCallCount++; return Task.FromResult(Fb2ProbeResult.Success); });

        viewModel.Fb2CngExecutablePath = "fb2cng.exe";

        Assert.Equal(0, probeCallCount);
        Assert.False(viewModel.IsConverterProbeInProgress);
        Assert.True(viewModel.HasConverterValidationError);
        Assert.False(viewModel.SavePreferencesCommand.CanExecute(null));
    }

    [Fact]
    public async Task ShellViewModel_ConverterProbeIsCancelledWhenPathChangesAgain()
    {
        var firstProbeCts = new CancellationTokenSource();
        var firstProbeStarted = new TaskCompletionSource();
        var viewModel = MakeTestShellViewModel(probe: async (_, ct) =>
        {
            firstProbeStarted.TrySetResult();
            await Task.Delay(Timeout.Infinite, ct);
            return Fb2ProbeResult.Success;
        });

        viewModel.Fb2CngExecutablePath = @"C:\fake\fb2cng.exe";
        await firstProbeStarted.Task;
        var firstTask = viewModel.WaitForConverterProbeAsync();

        // Change path — cancels the in-flight probe
        viewModel.Fb2CngExecutablePath = string.Empty;

        await firstTask; // should complete quickly after cancellation

        Assert.False(viewModel.IsConverterProbeInProgress);
        Assert.False(viewModel.HasConverterValidationError);
    }

    [Fact]
    public async Task ShellViewModel_DisposeCancelsPendingLibraryBrowserRefresh()
    {
        var catalogService = new RecordingLibraryCatalogService();
        using var viewModel = new ShellViewModel(
            new ShellSession(
                hostLifetime: NullLifetime.Instance,
                hostOptions: new CoreHostLaunchOptions
                {
                    ExecutablePath = @"C:\fake\host.exe",
                    PipePath = @"\\.\pipe\test",
                    LibraryRoot = @"C:\fake\lib"
                },
                importJobs: new FakeImportJobsService(),
                libraryCatalog: catalogService));

        viewModel.LibraryBrowser.SearchText = "road";
        viewModel.Dispose();

        await Task.Delay(400);

        Assert.Null(catalogService.LastRequest);
    }

    [Fact]
    public void ShellViewModel_DiagnosticsPathsRemainStableAfterConstruction()
    {
        var originalStatePath = Environment.GetEnvironmentVariable(RuntimeEnvironment.UiStateFileEnvVar);
        var originalPreferencesPath = Environment.GetEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar);
        var sandboxRoot = Path.Combine(Path.GetTempPath(), "librova-ui-viewmodels", $"{Guid.NewGuid():N}");
        Directory.CreateDirectory(sandboxRoot);

        try
        {
            var initialStatePath = Path.Combine(sandboxRoot, "initial-state.json");
            var initialPreferencesPath = Path.Combine(sandboxRoot, "initial-preferences.json");
            Environment.SetEnvironmentVariable(RuntimeEnvironment.UiStateFileEnvVar, initialStatePath);
            Environment.SetEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar, initialPreferencesPath);

            using var viewModel = MakeTestShellViewModel();
            Assert.Equal(initialStatePath, viewModel.UiStateFilePath);
            Assert.Equal(initialPreferencesPath, viewModel.UiPreferencesFilePath);

            Environment.SetEnvironmentVariable(RuntimeEnvironment.UiStateFileEnvVar, Path.Combine(sandboxRoot, "changed-state.json"));
            Environment.SetEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar, Path.Combine(sandboxRoot, "changed-preferences.json"));

            Assert.Equal(initialStatePath, viewModel.UiStateFilePath);
            Assert.Equal(initialPreferencesPath, viewModel.UiPreferencesFilePath);
        }
        finally
        {
            Environment.SetEnvironmentVariable(RuntimeEnvironment.UiStateFileEnvVar, originalStatePath);
            Environment.SetEnvironmentVariable(RuntimeEnvironment.UiPreferencesFileEnvVar, originalPreferencesPath);
            try
            {
                Directory.Delete(sandboxRoot, true);
            }
            catch (IOException)
            {
            }
            catch (UnauthorizedAccessException)
            {
            }
        }
    }

    private static ShellViewModel MakeTestShellViewModel(
        Func<string, CancellationToken, Task<Fb2ProbeResult>>? probe = null)
    {
        var session = new ShellSession(
            hostLifetime: NullLifetime.Instance,
            hostOptions: new CoreHostLaunchOptions
            {
                ExecutablePath = @"C:\fake\host.exe",
                PipePath = @"\\.\pipe\test",
                LibraryRoot = @"C:\fake\lib"
            },
            importJobs: new FakeImportJobsService());

        return new ShellViewModel(session, converterProbe: probe);
    }

    private static void CreateDirectoryJunction(string junctionPath, string targetPath)
    {
        var startInfo = new ProcessStartInfo
        {
            FileName = "cmd.exe",
            Arguments = $"/c mklink /J \"{junctionPath}\" \"{targetPath}\"",
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardError = true,
            RedirectStandardOutput = true
        };

        using var process = Process.Start(startInfo);
        Assert.NotNull(process);
        process!.WaitForExit();
        var output = process.StandardOutput.ReadToEnd();
        var error = process.StandardError.ReadToEnd();
        Assert.True(
            process.ExitCode == 0,
            $"Failed to create junction. ExitCode={process.ExitCode} Output={output} Error={error}");
    }

    private sealed class NullLifetime : IAsyncDisposable
    {
        public static readonly NullLifetime Instance = new();
        public ValueTask DisposeAsync() => ValueTask.CompletedTask;
    }

    // ── Fb2ProbeResult.BuildValidationMessage coverage ──────────────────────────────────────

    [Fact]
    public void Fb2ProbeResult_Succeeded_ReturnsEmptyMessage()
    {
        Assert.Equal(string.Empty, Fb2ProbeResult.Success.BuildValidationMessage());
    }

    [Fact]
    public void Fb2ProbeResult_ExecutableNotFound_ReturnsNonEmptyMessage()
    {
        var result = new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.ExecutableNotFound };
        Assert.False(string.IsNullOrWhiteSpace(result.BuildValidationMessage()));
    }

    [Fact]
    public void Fb2ProbeResult_ExecutionFailed_WithDetail_IncludesExitCode()
    {
        var result = new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.ExecutionFailed, Detail = "2" };
        Assert.Contains("2", result.BuildValidationMessage());
    }

    [Fact]
    public void Fb2ProbeResult_ExecutionFailed_WithoutDetail_ReturnsNonEmptyMessage()
    {
        var result = new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.ExecutionFailed };
        Assert.False(string.IsNullOrWhiteSpace(result.BuildValidationMessage()));
    }

    [Fact]
    public void Fb2ProbeResult_NoOutputProduced_ReturnsNonEmptyMessage()
    {
        var result = new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.NoOutputProduced };
        Assert.False(string.IsNullOrWhiteSpace(result.BuildValidationMessage()));
    }

    [Fact]
    public void Fb2ProbeResult_TimedOut_ReturnsNonEmptyMessage()
    {
        var result = new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.TimedOut };
        Assert.False(string.IsNullOrWhiteSpace(result.BuildValidationMessage()));
    }

    [Fact]
    public void Fb2ProbeResult_ProbeError_ReturnsNonEmptyMessage()
    {
        var result = new Fb2ProbeResult { Outcome = Fb2ProbeOutcome.ProbeError };
        Assert.False(string.IsNullOrWhiteSpace(result.BuildValidationMessage()));
    }
}
