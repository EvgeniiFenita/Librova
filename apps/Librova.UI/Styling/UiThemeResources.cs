using Avalonia;
using Avalonia.Media;
using Avalonia.Media.Immutable;
using Avalonia.Styling;
using System;
using System.Collections.Generic;

namespace Librova.UI.Styling;

internal static class UiThemeResources
{
    private static readonly ThemeVariant FallbackThemeVariant = ThemeVariant.Dark;
    private static readonly IReadOnlyDictionary<string, IBrush> FallbackBrushes = new Dictionary<string, IBrush>(StringComparer.Ordinal)
    {
        ["AppSurfaceMutedBrush"] = CreateSolidColorBrush("#1C160C"),
        ["AppBorderBrush"] = CreateSolidColorBrush("#2A200E"),
        ["AppSurfaceAltBrush"] = CreateSolidColorBrush("#221A0E"),
        ["AppAccentBrush"] = CreateSolidColorBrush("#F5A623"),
        ["AppMatteBrush"] = CreateSolidColorBrush("#0A0700")
    };

    private static readonly IReadOnlyList<LinearGradientBrush> FallbackCoverPlaceholderBrushes =
    [
        CreateGradientBrush("#6C4B3A", "#D9A066"),
        CreateGradientBrush("#5C3A28", "#C4855A"),
        CreateGradientBrush("#5A2F3D", "#C18E78"),
        CreateGradientBrush("#40523B", "#B6C58E"),
        CreateGradientBrush("#6B4C1E", "#D2B87A")
    ];

    public static IBrush AppSurfaceMutedBrush => GetBrush("AppSurfaceMutedBrush");
    public static IBrush AppBorderBrush => GetBrush("AppBorderBrush");
    public static IBrush AppSurfaceAltBrush => GetBrush("AppSurfaceAltBrush");
    public static IBrush AppAccentBrush => GetBrush("AppAccentBrush");
    public static IBrush AppMatteBrush => GetBrush("AppMatteBrush");
    public static IReadOnlyList<LinearGradientBrush> CoverPlaceholderBrushes => FallbackCoverPlaceholderBrushes;

    private static IBrush GetBrush(string key)
    {
        if (Application.Current?.TryGetResource(key, Application.Current.ActualThemeVariant, out var value) == true
            && value is IBrush brush)
        {
            return CloneBrush(brush);
        }

        if (Application.Current?.TryGetResource(key, FallbackThemeVariant, out value) == true
            && value is IBrush fallbackThemeBrush)
        {
            return CloneBrush(fallbackThemeBrush);
        }

        return FallbackBrushes[key];
    }

    private static IBrush CloneBrush(IBrush brush) =>
        brush switch
        {
            ISolidColorBrush solidBrush => new ImmutableSolidColorBrush(solidBrush.Color, solidBrush.Opacity),
            _ => brush
        };

    private static ImmutableSolidColorBrush CreateSolidColorBrush(string colorHex) =>
        new(Color.Parse(colorHex));

    private static LinearGradientBrush CreateGradientBrush(string startColorHex, string endColorHex) =>
        new()
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative),
            GradientStops =
            [
                new GradientStop(Color.Parse(startColorHex), 0),
                new GradientStop(Color.Parse(endColorHex), 1)
            ]
        };
}
