import UIKit

/*
 * Loads the images the templates show. The loader the library starts with fetches each url
 * once, decodes it off the main thread at the size its view shows it at, and keeps the decoded
 * images in memory. An app that has its own image stack subclasses this, overrides load and
 * cancel, and sets shared.
 */
open class ShadowListUtilsImageLoader {
  /*
   * The loader every image view of the library uses.
   */
  public static var shared = ShadowListUtilsImageLoader()

  /*
   * One fetch and the views that wait for it.
   */
  private final class Waiting {
    var task: URLSessionDataTask?
    var views: [ObjectIdentifier: () -> ShadowListUtilsImageView?] = [:]
  }

  private let cache = NSCache<NSString, UIImage>()
  private let session: URLSession
  private let decodeQueue = DispatchQueue(label: "com.shadowlist.utils.image", qos: .userInitiated, attributes: .concurrent)

  /*
   * The fetch of each image, and the image each view waits for. Both are only touched on the
   * main thread.
   */
  private var waiting: [String: Waiting] = [:]
  private var waitingKeys: [ObjectIdentifier: String] = [:]

  public init() {
    let configuration = URLSessionConfiguration.default
    configuration.urlCache = URLCache(memoryCapacity: Self.memoryCapacity, diskCapacity: Self.diskCapacity)
    configuration.requestCachePolicy = .returnCacheDataElseLoad
    session = URLSession(configuration: configuration)
    cache.totalCostLimit = Self.decodedCapacity
  }

  /*
   * Sets the view's image when the image arrives, unless the view's url changed meanwhile.
   * Called on the main thread, once the view has its size.
   */
  open func load(_ url: String, into view: ShadowListUtilsImageView) {
    let size = view.bounds.size
    let key = Self.key(url, size)
    if let image = cache.object(forKey: key as NSString) {
      view.image = image
      return
    }
    guard let address = URL(string: url) else { return }
    let identifier = ObjectIdentifier(view)
    waitingKeys[identifier] = key
    if let entry = waiting[key] {
      entry.views[identifier] = { [weak view] in view }
      return
    }
    let entry = Waiting()
    entry.views[identifier] = { [weak view] in view }
    waiting[key] = entry
    let scale = view.window?.screen.scale ?? ShadowListUtilsPlatform.displayScale
    let task = session.dataTask(with: address) { [weak self] data, _, _ in
      self?.decodeQueue.async {
        var image: UIImage?
        if let data, let source = UIImage(data: data) {
          let pixels = CGSize(width: size.width * scale, height: size.height * scale)
          image = source.preparingThumbnail(of: Self.fill(source.size, into: pixels)) ?? source.preparingForDisplay()
        }
        DispatchQueue.main.async { self?.finish(key, url: url, image: image) }
      }
    }
    entry.task = task
    task.resume()
  }

  /*
   * Ends what the view waits for. A fetch nobody waits for any more is aborted. Called on the
   * main thread.
   */
  open func cancel(_ view: ShadowListUtilsImageView) {
    let identifier = ObjectIdentifier(view)
    guard let key = waitingKeys.removeValue(forKey: identifier), let entry = waiting[key] else { return }
    entry.views.removeValue(forKey: identifier)
    if entry.views.isEmpty {
      entry.task?.cancel()
      waiting.removeValue(forKey: key)
    }
  }

  /*
   * Keep the decoded image and hand it to every view still waiting for this url.
   */
  private func finish(_ key: String, url: String, image: UIImage?) {
    guard let entry = waiting.removeValue(forKey: key) else { return }
    if let image {
      let cost = Int(image.size.width * image.size.height * image.scale * image.scale) * 4
      cache.setObject(image, forKey: key as NSString, cost: cost)
    }
    for (identifier, resolve) in entry.views {
      if waitingKeys[identifier] == key {
        waitingKeys.removeValue(forKey: identifier)
      }
      if let view = resolve(), view.url == url, let image {
        view.image = image
      }
    }
  }

  private static func key(_ url: String, _ size: CGSize) -> String {
    "\(url)#\(Int(size.width))x\(Int(size.height))"
  }

  /*
   * The smallest size with the source's aspect that covers the target.
   */
  private static func fill(_ source: CGSize, into target: CGSize) -> CGSize {
    guard source.width > 0, source.height > 0 else { return target }
    let scale = max(target.width / source.width, target.height / source.height)
    return CGSize(width: ceil(source.width * scale), height: ceil(source.height * scale))
  }

  private static let memoryCapacity = 32 << 20
  private static let diskCapacity = 512 << 20
  private static let decodedCapacity = 96 << 20
}
