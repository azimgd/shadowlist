import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
  func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
    Rendering.apply()
    TextCheck.run()
    return true
  }
}

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {
  var window: UIWindow?

  func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
    guard let scene = scene as? UIWindowScene else { return }
    let window = UIWindow(windowScene: scene)
    switch UserDefaults.standard.string(forKey: "SLTheme") {
    case "light": window.overrideUserInterfaceStyle = .light
    case "dark": window.overrideUserInterfaceStyle = .dark
    default: break
    }
    let navigation = UINavigationController(rootViewController: HomeScreen())
    navigation.navigationBar.prefersLargeTitles = true
    navigation.navigationBar.tintColor = AppTheme.colors.accent
    let opaque = UINavigationBarAppearance()
    opaque.configureWithOpaqueBackground()
    opaque.backgroundColor = AppTheme.colors.background
    opaque.shadowColor = .clear
    if let route = Settings.route, let example = ExampleSection.example(route: route) {
      let screen = example.make()
      screen.navigationItem.largeTitleDisplayMode = .never
      screen.navigationItem.standardAppearance = opaque
      screen.navigationItem.scrollEdgeAppearance = opaque
      navigation.pushViewController(screen, animated: false)
    }
    window.rootViewController = navigation
    window.makeKeyAndVisible()
    self.window = window
  }
}
